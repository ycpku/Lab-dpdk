#include "shared.h"
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_mempool_trace.h>
#include <rte_mempool_trace_fp.h>
#include <rte_memzone.h>
#include <rte_port.h>
#include <rte_lpm.h>
#include <rte_config.h>
#include <rte_port_ethdev.h>
#include <arpa/inet.h>
#include <vector>
std::vector <struct rte_mbuf*> tx_buffer[10005];
static struct rte_eth_conf port_conf = {
    .rxmode = {
        .offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM,
    },
    .txmode = {
        .mq_mode = RTE_ETH_MQ_TX_NONE,
    },
};
struct rte_lpm* rule_table=NULL;
void userInit(int argc, char **argv) {
    struct rte_eth_dev_info dev_info;
    int portid;
    rte_eal_init(argc, argv);
    struct rte_mempool** mempool;
    struct rte_ether_addr ports_eth_addr[10005];
    uint16_t queueid=0;
    RTE_ETH_FOREACH_DEV(portid) {
        /** Create Mempool **/
        int socketid = rte_socket_id();
        char s[128];
        snprintf(s, sizeof(s), "mbuf_pool_%d", portid);
        mempool[portid] = rte_pktmbuf_pool_create(
            s, 512, MEMPOOL_CACHE_SIZE, 0,
            RTE_MBUF_DEFAULT_BUF_SIZE, socketid
        );
        /** Init Device Settings **/
        struct rte_eth_conf local_port_conf = port_conf;
        rte_eth_dev_configure(portid, 1, 1, &local_port_conf);
        rte_eth_macaddr_get(portid, &ports_eth_addr[portid]);
        rte_eth_dev_info_get(portid, &dev_info);
        uint16_t nb_rxd = 128;
        uint16_t nb_txd = 128;
        rte_eth_dev_adjust_nb_rx_tx_desc(portid, &nb_rxd, &nb_txd);
        /** Init RX Queue **/
        rte_eth_rx_queue_setup(portid, queueid,
                                nb_rxd, socketid,
                                &dev_info.default_rxconf,
                                mempool[portid]);
        /** Init TX Queue **/
        rte_eth_tx_queue_setup(portid, 0, nb_txd, socketid, &dev_info.default_txconf);
        /** Start Dev **/
        rte_eth_promiscuous_enable(portid);
        rte_eth_dev_start(portid);
    }
}

int userLoop() {
    int portid;
    RTE_ETH_FOREACH_DEV(portid) {
        if (tx_buffer[portid].size()) {
            int sent = 0;
            while (sent < tx_buffer[portid].size()) {
                auto n_sent = rte_eth_tx_burst(portid, 0,
                tx_buffer[portid].data() + sent,
                tx_buffer[portid].size() - sent);
                sent += n_sent;
            }
        }
        tx_buffer[portid].clear();
    }
    RTE_ETH_FOREACH_DEV(portid) {
        struct rte_mbuf* pkts[32];
        auto n_pkt = rte_eth_rx_burst(portid, 0, pkts, 32);
        for (int i = 0; i < n_pkt; i ++) {
            auto pkt = pkts[i];
            auto eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr*);
            auto ipv4_hdr = (struct rte_ipv4_hdr *)(eth_hdr + 1);
            ipv4_hdr->time_to_live--;
            ipv4_hdr->hdr_checksum++;
            uint32_t dst_ip = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
            auto next_hop = userGetNextHop(dst_ip);
            if (next_hop == -1)
                rte_pktmbuf_free(pkt);
            else
                tx_buffer[next_hop].push_back(pkt);
        }
    }
}

int userAddLPMRule(uint32_t dst_ip, uint8_t cidr, uint8_t dst_port) {
    if (!rule_table) {
        char s[64];
        snprintf(s, sizeof(s), "IPV4_L3FWD_LPM");
        struct rte_lpm_config config_ipv4;
        config_ipv4.max_rules = IPV4_L3FWD_LPM_MAX_RULES;
        config_ipv4.number_tbl8s = IPV4_L3FWD_LPM_NUMBER_TBL8S;
        config_ipv4.flags = 0;
        int socketid = 100;
        rule_table = rte_lpm_create(s, socketid, &config_ipv4);
    }
    if (!rule_table) return -1;
    return rte_lpm_add(rule_table, dst_ip, cidr, dst_port);
}

int userDelLPMRule(uint32_t dst_ip, uint8_t cidr) {
    if (!rule_table) return -1;
    return rte_lpm_delete(rule_table, dst_ip, cidr);
}

int userGetNextHop(uint32_t dst_ip) {
    if (!rule_table)
        return -1;
    uint32_t ret;
    if (rte_lpm_lookup(rule_table, dst_ip, &ret) == 0) return (int)ret;
        return -1;
}
