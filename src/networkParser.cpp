//
// Created by lw96 on 06/04/18.
//
#include <boost/graph/graphml.hpp>
#include <boost/graph/directed_graph.hpp>
#include <boost/graph/labeled_graph.hpp>
#include <boost/graph/vertex_and_edge_range.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/property_maps/container_property_map.hpp>
#include <boost/graph/graphviz.hpp>
#include <unistd.h>
#include <limits.h>
#include <algorithm>
#include <set>
#include "logging.h"

using namespace boost;
using namespace std;

struct DotVertex {
    std::string name;
    std::string label;
};

typedef property < vertex_name_t, std::pair<std::string,std::string>,
        property < vertex_color_t, float > > vertex_p;
typedef property < edge_weight_t, double > edge_p;
typedef property < graph_name_t, std::string > graph_p;
typedef adjacency_list < vecS, vecS, directedS,
        DotVertex, edge_p, graph_p > graph_t;
graph_t networkGraph(0);
void readGraph(std::string path){

    // Construct an empty graph and prepare the dynamic_property_maps.
    dynamic_properties dp;
    dp.property("node_id",     boost::get(&DotVertex::name,        networkGraph));
    dp.property("label",       boost::get(&DotVertex::label,       networkGraph));

    property_map<graph_t, edge_weight_t>::type weight =
            get(edge_weight, networkGraph);
    dp.property("weight",weight);

    boost::ref_property_map<graph_t*,std::string>
            gname(get_property(networkGraph,graph_name));
    dp.property("name",gname);

    std::ifstream dot(path);
    bool status = read_graphviz(dot,networkGraph,dp,"node_id");

    long i = *(networkGraph.vertex_set().begin());
    vector<string> vertexNames;
    [&](){for(auto iter = networkGraph.vertex_set().begin(); iter<networkGraph.vertex_set().end(); iter++, i=*iter) vertexNames.push_back(networkGraph.m_vertices[i].m_property.label);}();
    //boost::write_graphviz(std::cout, networkGraph, make_label_writer(vertexNames.data()));
}
string getIdentity(){
    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);
    return string(hostname).substr(0, strcspn(hostname, "."));
}
std::pair<std::string, int> getHost(){
    int ourVertex;
    for(int i = 0; i < networkGraph.m_vertices.size(); i++){
        string label = networkGraph.m_vertices[i].m_property.label;
        if(label.substr(0, strcspn(label.data(), ":")) == getIdentity()){
            ourVertex = i;
        }
    }
    string host = networkGraph.m_vertices[ourVertex].m_property.label;
    string addr = host.substr(0, strcspn(host.data(), ":"));
    int port = atoi(host.substr(strcspn(host.data(), ":")+1).data());
    pair<string, int> hnode(addr, port);
    return hnode;
}
set<pair<string, int>> getNeighbourHosts(){
    set<pair<string,int>> hosts;
    int ourVertex;
    for(int i = 0; i < networkGraph.m_vertices.size(); i++){
        string label = networkGraph.m_vertices[i].m_property.label;
        if(label.substr(0, strcspn(label.data(), ":")) == getIdentity()){
            ourVertex = i;
        }
    }
    BOOST_LOG_SEV(Logger::getLogger(), debug)<< "NetworkParser: Our Node is Vertex " <<ourVertex <<std::endl;
    typename graph_traits < graph_t >::out_edge_iterator ei, ei_end;

    for (boost::tie(ei, ei_end) = out_edges(ourVertex, networkGraph); ei != ei_end; ++ei) {
        auto source = boost::source ( *ei, networkGraph );
        auto target = boost::target ( *ei, networkGraph );
        BOOST_LOG_SEV(Logger::getLogger(), debug)<< "There is an edge from " << source <<  " to " << target <<std::endl;
        string label = networkGraph.m_vertices[target].m_property.label;
        string addr = label.substr(0, strcspn(label.data(), ":"));
        int port = atoi(label.substr(strcspn(label.data(), ":")+1).data());
        pair<string, int> node(addr, port);
        hosts.insert(node);
    }
    return hosts;
}
set<pair<string, int>> getAllHosts() {
    set<pair<string,int>> hosts;
    int ourVertex;
    for(int i = 0; i < networkGraph.m_vertices.size(); i++){
        string label = networkGraph.m_vertices[i].m_property.label;
        if(label.substr(0, strcspn(label.data(), ":")) == getIdentity()){
            ourVertex = i;
        }
    }
    typename graph_traits < graph_t >::out_edge_iterator ei, ei_end;

    for(auto v : networkGraph.m_vertices) {
        string label = v.m_property.label;
        if(!(label.substr(0, strcspn(label.data(), ":")) == getIdentity())) {//not us
            string addr = label.substr(0, strcspn(label.data(), ":"));
            int port = atoi(label.substr(strcspn(label.data(), ":") + 1).data());
            pair<string, int> node(addr, port);
            hosts.insert(node);
        }
    }
    return hosts;
};