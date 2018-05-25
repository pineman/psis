#pragma once

// Region struct
struct region {
	char *data; // Always changing on parent read()
	uint32_t data_size;
	pthread_rwlock_t rwlock; // Protects `data` and `data_size`
};

void init_regions(void);
void destroy_regions(void);

bool do_copy(int sockfd, uint8_t region, uint32_t data_size, char **data, bool copy_down);

void copy_to_parent(uint8_t region, uint32_t data_size, char *data);

void update_region(uint8_t region, uint32_t data_size, char **data);
void copy_to_children(uint8_t region);

bool paste_region_app(uint8_t region, int app);
