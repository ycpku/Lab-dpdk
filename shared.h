#ifndef __SHARED_H
#define __SHARED_H

#include <cstdint>
#include <vector>
#include <rte_mbuf.h>
#define MEMPOOL_CACHE_SIZE 6
#define IPV4_L3FWD_LPM_MAX_RULES 1024
#define IPV4_L3FWD_LPM_NUMBER_TBL8S (1<<16)
#ifdef __cplusplus
extern "C" {
#endif
extern struct rte_lpm* rule_table; 
extern std::vector <struct rte_mbuf*> tx_buffer[10005];

extern void userInit(int argc, char **argv);
extern int userLoop();

extern int userAddLPMRule(uint32_t dst_ip, uint8_t cidr, uint8_t dst_port);
extern int userDelLPMRule(uint32_t dst_ip, uint8_t cidr);
extern int userGetNextHop(uint32_t dst_ip);

#ifdef __cplusplus
}
#endif

#endif
