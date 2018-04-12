import sys
from igraph import *

def mkDot(hosts):
    dot = 'digraph {\ngraph [name="networkGraph"]\n'
    nameIdMap = {}
    id = chr(65) # 'A'
    for h in hosts:
        dot += str(id) + '[label="'+h+':12345"];\n'
        nameIdMap[h]=id
        id=chr(ord(id)+1)
#    for h in hosts:
#        for h2 in hosts:
#            if h != h2:
#                dot+= nameIdMap[h] +"->"+nameIdMap[h2]+";\n"
    for src, tgtList in mkFFGraph(hosts).items():
        for tgt in tgtList:
            dot+= nameIdMap[src] +"->"+nameIdMap[tgt]+";\n"
    dot+="}"
    return dot
def mkFFGraph(hosts):
    hostId = {}
    i = 0
    for host in hosts:
        hostId[i]=host
        i+=1
    g = Graph.Forest_Fire(i, 0.5, 0)
    layout = g.layout("kk")
    plot(g, layout = layout)
    adjList = g.get_adjlist(mode=ALL)
    connections = {}
    for v in range(len(adjList)):
        cNode = hostId[v]
        connections[cNode]=[]
        adj = adjList[v]
        for vt in adj:
            connections[cNode].append(hostId[vt])
    return connections

def mkPssh(hosts):
    pssh = ""
    for h in hosts:
        pssh+=h+"\n"
    return pssh
def genBuildCmd():
    cmd = "pssh -t 240 -h psshFile " # allow up to 4 min for build
    cmd += '"mkdir -p /cs/scratch/$(whoami)/cmake-build-debug && cd /cs/scratch/$(whoami)/cmake-build-debug && cmake ~/CLionProjects/cs4103 && make -j6 -l4"'
    return cmd
def genRunCmd():
    cmd = "pssh -t 180 -P -h psshFile " #run for 3 min, display live program output
    cmd += '"killall cs4103; cd /cs/scratch/$(whoami)/cmake-build-debug && ./cs4103"'
    return cmd
if __name__=='__main__':
    if len(sys.argv) < 2:
        print("usage: "+sys.argv[0]+" host1 host2 [host3 ... hostn]")
        exit()
    hosts = sys.argv[1:]
    print("Generating network using hosts: ")
    for h in hosts:
        print(h)
    dot = mkDot(hosts)
    pssh = mkPssh(hosts)
    buildcmd = genBuildCmd()
    runcmd = genRunCmd()
    with open('psshFile','w') as f:
        f.write(pssh)
    with open('generatedNetwork.dot','w') as f:
        f.write(dot)
    with open('buildPSSH.sh','w') as f:
        f.write(buildcmd)
    with open('runPSSH.sh','w') as f:
        f.write(runcmd)
