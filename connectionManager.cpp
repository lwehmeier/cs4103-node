//
// Created by lw96 on 06/04/18.
//

#include "connectionManager.h"


using boost::asio::deadline_timer;
using boost::asio::ip::tcp;

//
// This class manages socket timeouts by applying the concept of a deadline.
// Some asynchronous operations are given deadlines by which they must complete.
// Deadlines are enforced by an "actor" that persists for the lifetime of the
// client object:
//
//  +----------------+
//  |                |
//  | check_deadline |<---+
//  |                |    |
//  +----------------+    | async_wait()
//              |         |
//              +---------+
//
// If the deadline actor determines that the deadline has expired, the socket
// is closed and any outstanding operations are consequently cancelled.
//
// Connection establishment involves trying each endpoint in turn until a
// connection is successful, or the available endpoints are exhausted. If the
// deadline actor closes the socket, the connect actor is woken up and moves to
// the next endpoint.
//
//  +---------------+
//  |               |
//  | start_connect |<---+
//  |               |    |
//  +---------------+    |
//           |           |
//  async_-  |    +----------------+
// connect() |    |                |
//           +--->| handle_connect |
//                |                |
//                +----------------+
//                          :
// Once a connection is     :
// made, the connect        :
// actor forks in two -     :
//                          :
// an actor for reading     :       and an actor for
// inbound messages:        :       sending heartbeats:
//                          :
//  +------------+          :          +-------------+
//  |            |<- - - - -+- - - - ->|             |
//  | start_read |                     | start_write |<---+
//  |            |<---+                |             |    |
//  +------------+    |                +-------------+    | async_wait()
//          |         |                        |          |
//  async_- |    +-------------+       async_- |    +--------------+
//   read_- |    |             |       write() |    |              |
//  until() +--->| handle_read |               +--->| handle_write |
//               |             |                    |              |
//               +-------------+                    +--------------+
//
// The input actor reads messages from the socket, where messages are delimited
// by the newline character. The deadline for a complete message is 30 seconds.
//
// The heartbeat actor sends a heartbeat (a message that consists of a single
// newline character) every 10 seconds. In this example, no deadline is applied
// message sending.
//
    // Called by the user of the client class to initiate the connection process.
    // The endpoint iterator will have been obtained using a tcp::resolver.
    void client::start(tcp::resolver::iterator endpoint_iter)
    {
        // Start the connect actor.
        start_connect(endpoint_iter);

        // Start the deadline actor. You will note that we're not setting any
        // particular deadline here. Instead, the connect and input actors will
        // update the deadline prior to each asynchronous operation.
        deadline_.async_wait(boost::bind(&client::check_deadline, this));
    }

    // This function terminates all the actors to shut down the connection. It
    // may be called by the user of the client class, or by the class itself in
    // response to graceful termination or an unrecoverable error.
    void client::stop()
    {
        stopped_ = true;
        socket_.close();
        deadline_.cancel();
        heartbeat_timer_.cancel();
    }
    void client::start_connect(tcp::resolver::iterator endpoint_iter)
    {
        if (endpoint_iter != tcp::resolver::iterator())
        {
            std::cout << "Trying " << endpoint_iter->endpoint() << "...\n";

            // Set a deadline for the connect operation.
            deadline_.expires_from_now(boost::posix_time::seconds(60));

            // Start the asynchronous connect operation.
            socket_.async_connect(endpoint_iter->endpoint(),
                                  boost::bind(&client::handle_connect,
                                              this, _1, endpoint_iter));
        }
        else
        {
            // There are no more endpoints to try. Shut down the client.
            stop();
        }
    }

    void client::handle_connect(const boost::system::error_code& ec,
                        tcp::resolver::iterator endpoint_iter)
    {
        if (stopped_)
            return;

        // The async_connect() function automatically opens the socket at the start
        // of the asynchronous operation. If the socket is closed at this time then
        // the timeout handler must have run first.
        if (!socket_.is_open())
        {
            std::cout << "Connect timed out\n";

            // Try the next available endpoint.
            start_connect(++endpoint_iter);
        }

            // Check if the connect operation failed before the deadline expired.
        else if (ec)
        {
            std::cout << "Connect error: " << ec.message() << "\n";

            // We need to close the socket used in the previous connection attempt
            // before starting a new one.
            socket_.close();

            // Try the next available endpoint.
            start_connect(++endpoint_iter);
        }

            // Otherwise we have successfully established a connection.
        else
        {
            std::cout << "Connected to " << endpoint_iter->endpoint() << "\n";

            // Start the input actor.
            start_read();

            // Start the heartbeat actor.
            start_write();
        }
    }

    void client::start_read()
    {
        // Set a deadline for the read operation.
        deadline_.expires_from_now(boost::posix_time::seconds(30));

        // Start an asynchronous operation to read a newline-delimited message.
        boost::asio::async_read_until(socket_, input_buffer_, '\n',
                                      boost::bind(&client::handle_read, this, _1));
    }

    void client::handle_read(const boost::system::error_code& ec)
    {
        if (stopped_)
            return;

        if (!ec)
        {
            // Extract the newline-delimited message from the buffer.
            std::string line;
            std::istream is(&input_buffer_);
            std::getline(is, line);

            // Empty messages are heartbeats and so ignored.
            if (!line.empty())
            {
                std::cout << "Received: " << line << "\n";
            }

            start_read();
        }
        else
        {
            std::cout << "Error on receive: " << ec.message() << "\n";

            stop();
        }
    }

    void client::start_write()
    {
        if (stopped_)
            return;

        // Start an asynchronous operation to send a heartbeat message.
        boost::asio::async_write(socket_, boost::asio::buffer("\n", 1),
                                 boost::bind(&client::handle_write, this, _1));
    }

    void client::handle_write(const boost::system::error_code& ec)
    {
        if (stopped_)
            return;

        if (!ec)
        {
            // Wait 10 seconds before sending the next heartbeat.
            heartbeat_timer_.expires_from_now(boost::posix_time::seconds(10));
            heartbeat_timer_.async_wait(boost::bind(&client::start_write, this));
        }
        else
        {
            std::cout << "Error on heartbeat: " << ec.message() << "\n";

            stop();
        }
    }

    void client::check_deadline()
    {
        if (stopped_)
            return;

        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.
        if (deadline_.expires_at() <= deadline_timer::traits_type::now())
        {
            // The deadline has passed. The socket is closed so that any outstanding
            // asynchronous operations are cancelled.
            socket_.close();

            // There is no longer an active deadline. The expiry is set to positive
            // infinity so that the actor takes no action until a new deadline is set.
            deadline_.expires_at(boost::posix_time::pos_infin);
        }

        // Put the actor back to sleep.
        deadline_.async_wait(boost::bind(&client::check_deadline, this));
    }

//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//


using boost::asio::deadline_timer;
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

void channel::join(subscriber_ptr subscriber)
{
    subscribers_.insert(subscriber);
}

void channel::leave(subscriber_ptr subscriber)
{
    subscribers_.erase(subscriber);
}

void channel::deliver(const std::string& msg)
{
    std::for_each(subscribers_.begin(), subscribers_.end(),
                  boost::bind(&subscriber::deliver, _1, boost::ref(msg)));
}
//----------------------------------------------------------------------

//
// This class manages socket timeouts by applying the concept of a deadline.
// Some asynchronous operations are given deadlines by which they must complete.
// Deadlines are enforced by two "actors" that persist for the lifetime of the
// session object, one for input and one for output:
//
//  +----------------+                     +----------------+
//  |                |                     |                |
//  | check_deadline |<---+                | check_deadline |<---+
//  |                |    | async_wait()   |                |    | async_wait()
//  +----------------+    |  on input      +----------------+    |  on output
//              |         |  deadline                  |         |  deadline
//              +---------+                            +---------+
//
// If either deadline actor determines that the corresponding deadline has
// expired, the socket is closed and any outstanding operations are cancelled.
//
// The input actor reads messages from the socket, where messages are delimited
// by the newline character:
//
//  +------------+
//  |            |
//  | start_read |<---+
//  |            |    |
//  +------------+    |
//          |         |
//  async_- |    +-------------+
//   read_- |    |             |
//  until() +--->| handle_read |
//               |             |
//               +-------------+
//
// The deadline for receiving a complete message is 30 seconds. If a non-empty
// message is received, it is delivered to all subscribers. If a heartbeat (a
// message that consists of a single newline character) is received, a heartbeat
// is enqueued for the client, provided there are no other messages waiting to
// be sent.
//
// The output actor is responsible for sending messages to the client:
//
//  +--------------+
//  |              |<---------------------+
//  | await_output |                      |
//  |              |<---+                 |
//  +--------------+    |                 |
//      |      |        | async_wait()    |
//      |      +--------+                 |
//      V                                 |
//  +-------------+               +--------------+
//  |             | async_write() |              |
//  | start_write |-------------->| handle_write |
//  |             |               |              |
//  +-------------+               +--------------+
//
// The output actor first waits for an output message to be enqueued. It does
// this by using a deadline_timer as an asynchronous condition variable. The
// deadline_timer will be signalled whenever the output queue is non-empty.
//
// Once a message is available, it is sent to the client. The deadline for
// sending a complete message is 30 seconds. After the message is successfully
// sent, the output actor again waits for the output queue to become non-empty.
//

tcp_session::tcp_session(boost::asio::io_service& io_service, channel& ch)
            : channel_(ch),
              socket_(io_service),
              input_deadline_(io_service),
              non_empty_output_queue_(io_service),
              output_deadline_(io_service)
{
    input_deadline_.expires_at(boost::posix_time::pos_infin);
    output_deadline_.expires_at(boost::posix_time::pos_infin);

    // The non_empty_output_queue_ deadline_timer is set to pos_infin whenever
    // the output queue is empty. This ensures that the output actor stays
    // asleep until a message is put into the queue.
    non_empty_output_queue_.expires_at(boost::posix_time::pos_infin);
}

    tcp::socket& tcp_session::socket()
    {
        return socket_;
    }

    // Called by the server object to initiate the four actors.
    void tcp_session::start()
    {
        channel_.join(shared_from_this());

        start_read();

        input_deadline_.async_wait(
                boost::bind(&tcp_session::check_deadline,
                            shared_from_this(), &input_deadline_));

        await_output();

        output_deadline_.async_wait(
                boost::bind(&tcp_session::check_deadline,
                            shared_from_this(), &output_deadline_));
    }

    void tcp_session::stop()
    {
        channel_.leave(shared_from_this());

        boost::system::error_code ignored_ec;
        socket_.close(ignored_ec);
        input_deadline_.cancel();
        non_empty_output_queue_.cancel();
        output_deadline_.cancel();
    }

    bool tcp_session::stopped() const
    {
        return !socket_.is_open();
    }

    void tcp_session::deliver(const std::string& msg)
    {
        output_queue_.push_back(msg + "\n");

        // Signal that the output queue contains messages. Modifying the expiry
        // will wake the output actor, if it is waiting on the timer.
        non_empty_output_queue_.expires_at(boost::posix_time::neg_infin);
    }

    void tcp_session::start_read()
    {
        // Set a deadline for the read operation.
        input_deadline_.expires_from_now(boost::posix_time::seconds(30));

        // Start an asynchronous operation to read a newline-delimited message.
        boost::asio::async_read_until(socket_, input_buffer_, '\n',
                                      boost::bind(&tcp_session::handle_read, shared_from_this(), _1));
    }

    void tcp_session::handle_read(const boost::system::error_code& ec)
    {
        if (stopped())
            return;

        if (!ec)
        {
            // Extract the newline-delimited message from the buffer.
            std::string msg;
            std::istream is(&input_buffer_);
            std::getline(is, msg);

            if (!msg.empty())
            {
                channel_.deliver(msg);
            }
            else
            {
                // We received a heartbeat message from the client. If there's nothing
                // else being sent or ready to be sent, send a heartbeat right back.
                if (output_queue_.empty())
                {
                    output_queue_.push_back("\n");

                    // Signal that the output queue contains messages. Modifying the
                    // expiry will wake the output actor, if it is waiting on the timer.
                    non_empty_output_queue_.expires_at(boost::posix_time::neg_infin);
                }
            }

            start_read();
        }
        else
        {
            stop();
        }
    }

    void tcp_session::await_output()
    {
        if (stopped())
            return;

        if (output_queue_.empty())
        {
            // There are no messages that are ready to be sent. The actor goes to
            // sleep by waiting on the non_empty_output_queue_ timer. When a new
            // message is added, the timer will be modified and the actor will wake.
            non_empty_output_queue_.expires_at(boost::posix_time::pos_infin);
            non_empty_output_queue_.async_wait(
                    boost::bind(&tcp_session::await_output, shared_from_this()));
        }
        else
        {
            start_write();
        }
    }

    void tcp_session::start_write()
    {
        // Set a deadline for the write operation.
        output_deadline_.expires_from_now(boost::posix_time::seconds(30));

        // Start an asynchronous operation to send a message.
        boost::asio::async_write(socket_,
                                 boost::asio::buffer(output_queue_.front()),
                                 boost::bind(&tcp_session::handle_write, shared_from_this(), _1));
    }

    void tcp_session::handle_write(const boost::system::error_code& ec)
    {
        if (stopped())
            return;

        if (!ec)
        {
            output_queue_.pop_front();

            await_output();
        }
        else
        {
            stop();
        }
    }

    void tcp_session::check_deadline(deadline_timer* deadline)
    {
        if (stopped())
            return;

        // Check whether the deadline has passed. We compare the deadline against
        // the current time since a new asynchronous operation may have moved the
        // deadline before this actor had a chance to run.
        if (deadline->expires_at() <= deadline_timer::traits_type::now())
        {
            // The deadline has passed. Stop the session. The other actors will
            // terminate as soon as possible.
            stop();
        }
        else
        {
            // Put the actor back to sleep.
            deadline->async_wait(
                    boost::bind(&tcp_session::check_deadline,
                                shared_from_this(), deadline));
        }
    }

//----------------------------------------------------------------------

    server::server(boost::asio::io_service& io_service,
           const tcp::endpoint& listen_endpoint)
            : io_service_(io_service),
              acceptor_(io_service, listen_endpoint)
    {

        start_accept();
    }

    void server::start_accept()
    {
        tcp_session_ptr new_session(new tcp_session(io_service_, channel_));

        acceptor_.async_accept(new_session->socket(),
                               boost::bind(&server::handle_accept, this, new_session, _1));
    }

    void server::handle_accept(tcp_session_ptr session,
                       const boost::system::error_code& ec)
    {
        if (!ec)
        {
            session->start();
        }

        start_accept();
    }
