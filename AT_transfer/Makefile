#
# AT 
#
CC := gcc
SRC_FILES := AT_transfer.c
CFLAGS := -lpthread

EXTRA_FLAGS := -DCONFIG_SEND_ENGLIST_MESSAGE


all:
	$(CC) $(SRC_FILES) $(EXTRA_FLAGS) $(CFLAGS) -o AT

clean:
	rm AT
