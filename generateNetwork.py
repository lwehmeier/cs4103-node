import sys

def mkDot(hosts):
    dot = 'digraph {\ngraph [name="networkGraph"]\n'
    nameIdMap = {}
    id = chr(65) # 'A'
    for h in hosts:
        dot += str(id) + '[label="'+h+'"];\n'
        nameIdMap[h]=id
        id=chr(ord(id)+1)
    for h in hosts:
        for h2 in hosts:
            if h != h2:
                dot+= nameIdMap[h] +"->"+nameIdMap[h2]+";\n"
    dot+="}"
    return dot
def mkPssh(hosts):
    pssh = ""
    for h in hosts:
        pssh+=h+"\n"
    return pssh
def genBuildCmd():
    cmd = "pssh -t 240 -h psshFile " # allow up to 4 min for build
    cmd += '"mkdir -p /cs/scratch/lw96/cmake-build-debug && cd /cs/scratch/lw96/cmake-build-debug && cmake ~/CLionProjects/cs4103 && make -j6 -l4"'
    return cmd
def genRunCmd():
    cmd = "pssh -t 180 -P -h psshFile " #run for 3 min, display live program output
    cmd += '"cd /cs/scratch/lw96/cmake-build-debug && ./cs4103"'
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
