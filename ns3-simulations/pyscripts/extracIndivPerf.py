import argparse
import json

def getFlowKey(line, option=1):
    if option == 1:
        return "-".join(line.split('ns3::TcpHeader (')[1].split(' [')[0].split(' > '))
    elif option == 2:
        return "-".join(line.split('ns3::TcpHeader (')[1].split(' [')[0].split(' > ')[::-1])
    else:
        return -1

def getTimestamp(line):
    return line.split(' ')[1]

def getAck(line):
    return int(line.split(' Ack=')[1].split(' Win=')[0])

def main():
    argParser = argparse.ArgumentParser()
    argParser.add_argument("-f", "--filename", help="The name of the file containing the traces", type=str, required=True)

    args = argParser.parse_args()

    flowsTrack = {}

    with open(args.filename) as f:
        for line in f:

            portsTemp = line.split('ns3::TcpHeader')[1]
            portsTemp = portsTemp.split(' > ')
            srcPort = int(portsTemp[0].split('(')[-1])
            destPort = int(portsTemp[1].split(' ')[0])

            if ((srcPort >= 20000 and srcPort <= 21000) or (destPort >= 20000 and destPort <= 21000)):
                continue

            if line[0] == '-':
                if '[SYN]' in line:
                    flowKey = getFlowKey(line, option=1)

                    if flowKey not in flowsTrack:
                        flowsTrack[flowKey] = {
                            "s" : getTimestamp(line),
                            "acks" : {}
                        }

            elif line[0] == 'r':
                if 'Ack=' in line:
                    flowKey = getFlowKey(line, option=2)

                    ackNumber = getAck(line)

                    if ackNumber not in flowsTrack[flowKey]['acks']:
                        flowsTrack[flowKey]['acks'][ackNumber] = getTimestamp(line)
                
    for flow in flowsTrack:
        if type(flow) == str and '-' in flow:
            print("{0}\t{1}".format(flow ,json.dumps(flowsTrack[flow])))

main()
