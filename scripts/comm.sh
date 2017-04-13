get_ip4() {
	ip -o -4 addr show dev "$1" | awk '{print $4}' | cut -d/ -f1
}

get_ip6() {
	ip -o -6 addr show dev "$1" | awk '{print $4}' | cut -d/ -f1
}

