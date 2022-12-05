while getopts d:v:i:p: flag
    do 
        case "${flag}" in
        d) dev=${OPTARG};;
        v) vlan=${OPTARG};;
        i) ip=${OPTARG};;
        p) prio=${OPTARG};;
        esac
    done

sudo ip link del vlan$vlan

sudo ip link add link $dev name vlan$vlan type vlan id $vlan
sudo ip addr add $ip dev vlan$vlan


sudo ip link set vlan$vlan type vlan egress 0:$prio
sudo ip link set vlan$vlan type vlan egress 1:$prio
sudo ip link set vlan$vlan type vlan egress 2:$prio
sudo ip link set vlan$vlan type vlan egress 3:$prio
sudo ip link set vlan$vlan type vlan egress 4:$prio
sudo ip link set vlan$vlan type vlan egress 5:$prio
sudo ip link set vlan$vlan type vlan egress 6:$prio
sudo ip link set vlan$vlan type vlan egress 7:$prio
sudo ip link set vlan$vlan type vlan egress 8:$prio
sudo ip link set vlan$vlan up