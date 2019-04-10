#include "basic_classes.h"
#include <algorithm>
#include <tuple>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include "decode.h"

using namespace std;

// constructor 1
IP::IP(const string& raw_ip) {
	std::vector<string> vec = split(raw_ip, '/');
	std::vector<string>::iterator it = vec.begin();

	string raw_ip1 = *it;
	int raw_mask1 = std::stoi(*(++it));

	std::vector<std::string> vec1 = split(raw_ip1, '.');
	std::vector<std::string>::iterator it1 = vec1.begin();
	this->ip = 0;
	this->mask = 0;
	for (; it1 != vec1.end(); it1++) {
		uint8_t t = (uint8_t)std::stoi(*it1);
		this->ip = (this->ip << 8) + t;
	}
	// UINT32_MAX included in stdint.h
	this->mask = UINT32_MAX << (32 - raw_mask1);
	return;
}
// constructor 2
IP::IP(const string& raw_ip, int raw_mask) {
	std::vector<std::string> vec = split(raw_ip, '.');
	std::vector<std::string>::iterator it = vec.begin();
	this->ip = 0;
	this->mask = 0;
	for (; it != vec.end(); it++) {
		uint8_t t = (uint8_t)std::stoi(*it);
		this->ip = (this->ip << 8) + t;
	}
	// UINT32_MAX included in stdint.h
	this->mask = UINT32_MAX << (32 - raw_mask);
	return;
}
// constructor 3, mask should be 0~32
IP::IP(int ip, int raw_mask) {
	this->ip = ip;
	this->mask = UINT32_MAX << (32 - raw_mask);
	return;
}


char* IP::showAddr(){
	struct in_addr ip_addr;
	ip_addr.s_addr = htonl(this->ip);
	return inet_ntoa(ip_addr);
}


bool IP::operator<=(const IP& other) {
	if ((other.mask <= this->mask) && ((other.mask & other.ip) == (other.mask & this->ip))) {
		return true;
	}
	else {
		return false;
	}
}
/*for type IP, two IPs are equivalent only if they share them same ip, as well as mask*/
bool IP::operator==(const IP& other)const{
	return ((*this).ip == other.ip) && ((*this).mask == other.mask);
}
// reverse result of <=
bool IP::operator!=(const IP& other) {
	if ((other.mask <= this->mask) && ((other.mask & other.ip) == (other.mask & this->ip))) {
		return false;
	}
	else {
		return true;
	}
}


Flow::Flow(int* tag){
	this->headers[Tag] = tag;
	this->headers[Sip] = new IP(0, 0);
	this->headers[Dip] = new IP(0, 0);
	this->headers[Iplen] = new int(0);
}

Flow::Flow(u_char * packet, int totallength)  {
	/*Decoding*/	
	this->pkt = packet;
    int ethernet_header_length = 14;
	/* Header lengths in bytes */
    EtherHdr* e_hdr = (EtherHdr*) packet;
    if ( ntohs(e_hdr->ether_type) == 0x8100)	
	    ethernet_header_length = 14+4; /* For 802.1Q Virtual LAN */
    else
        ethernet_header_length = 14; /* For general wired */
	
    IPHdr * ip_hdr = (IPHdr*) (packet+ethernet_header_length);
    int src_addr = ntohl(ip_hdr->ip_src.s_addr);
	this->headers[Sip] = new IP(src_addr, 32);
    int dst_addr = ntohl(ip_hdr->ip_dst.s_addr);
	this->headers[Dip] = new IP(dst_addr, 32);

}
void Flow::clean() {
	/*Encoding*/	
}
//return IP/ int
void* & Flow::operator[]  (const string &field) {
	unordered_map<string, void *>::iterator it = field_value.find(field);
	if (it == field_value.end()) {
		ERROR_HANDLE("field "+field+" not in flow, now create a new entry, its tag is "+ to_string(*((int*)this->field_value["tag"])));
		//void * q = new string("error");
		Flow::field_value[field] = q;
		return q;
	}
	return it->second;
}

int Flow::matches(const string &field, const void * p) {
	if ((*this)[field] != NULL) {

	}
	return 1;
}
