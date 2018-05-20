#pragma once

// Region struct
struct region {
	char *data; // Always changing on parent read()
	uint32_t data_size;
	pthread_rwlock_t rwlock; // Protects `data` and `data_size`
};

void init_regions(void);
void destroy_regions(void);

void update_region(uint8_t region, uint32_t data_size, char **data);
void send_region_down(uint8_t region);
