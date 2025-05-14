#ifndef SYSCONFIGS_H
#define SYSCONFIGS_H

#include <QString>

typedef struct
{
    int log_level;

    int max_pt_number;
    int all_bytes_per_pt;
    int pkt_idx_byte_cnt;
    int min_row_interval_ms;
    int max_row_interval_ms;
    int start_row_idx;

    quint16 local_udp_port;
}sys_configs_struct_t;

extern sys_configs_struct_t g_sys_configs_block;

bool fill_sys_configs(QString *);

#endif // SYSCONFIGS_H
