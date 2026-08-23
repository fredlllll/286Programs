#define HDD_CYLS 820
#define HDD_HEADS 6
#define HDD_SPT 26
#define HDD_TOTAL_SECTORS ((unsigned long)HDD_CYLS*HDD_HEADS*HDD_SPT)

#define FLPD_CYLS 80
#define FLPD_HEADS 2
#define FLPD_SPT 18
#define FLPD_TOTAL_SECTORS (FLPD_CYLS*FLPD_HEADS*FLPD_SPT)
#define HEADER_LBAS 10
#define DISK_CAPACITY (FLPD_TOTAL_SECTORS-HEADER_LBAS)

#define BATCH_SECTORS 26        /* one full hdd track per ram buffer */
#define RETRY_HDD 16
#define RETRY_FLOPPY 4
#define REWRITE_ROUNDS 5        /* floppy: up to RETRY_FLOPPY*REWRITE_ROUNDS attempts per sector */
#define MAX_BAD 48              /* must match header layout space */
#define MAX_BAD_ALL 64
#define MAX_BAD_FLP 30          /* must match header layout space */
#define DISK_BAD_LIMIT 10       /* this many floppy failures = ask for fresh disk */