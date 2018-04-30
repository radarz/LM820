TOP_DIR=../..
PLAT_DIR=$(TOP_DIR)/linux
PROJ_DIR=.
OUTPUT_DIR=./obj

INC=-I$(TOP_DIR) -I$(PLAT_DIR) 

SRC_C=$(TOP_DIR)/xcom.c \
      $(TOP_DIR)/xcom_port.c \
      $(TOP_DIR)/util_crc.c \
      $(TOP_DIR)/util_timer.c \
      $(TOP_DIR)/util_log.c \
      $(TOP_DIR)/radar_ops.c \
      $(TOP_DIR)/radar_clt_main.c \
      $(PLAT_DIR)/hal_linux.c \
      $(PROJ_DIR)/main.c

SRC_CPP=$(PLAT_DIR)/display_linux.cpp \

OBJ_C=$(addprefix $(OUTPUT_DIR)/, $(notdir $(SRC_C:.c=.o)))
OBJ_CPP=$(addprefix $(OUTPUT_DIR)/, $(notdir $(SRC_CPP:.cpp=.o)))

CFLAG_C= -Wall -O2 $(INC)
PACKFLAG_C=
CFLAG_CPP= -Wall -O2 $(INC) -std=c++0x
PACKFLAG_CPP=

TARGET=radar_clt
TARLIB=
LIB=-lpthread -lstdc++

all: $(OUTPUT_DIR) $(OBJ_C) $(OBJ_CPP)
	$(CC) $(CFLAG) -o $(TARGET) $(OBJ_C) $(OBJ_CPP) $(LIB)

$(foreach obj_file,$(OBJ_C),$(eval $(obj_file):$(filter %/$(basename $(notdir $(obj_file))).c,$(SRC_C));$(CC) $(CFLAG_C) $(PACKFLAG_C) -c $$^ -o $$@))

$(foreach obj_file,$(OBJ_CPP),$(eval $(obj_file):$(filter %/$(basename $(notdir $(obj_file))).cpp,$(SRC_CPP));$(CC) $(CFLAG_CPP) $(PACKFLAG_CPP) -c $$^ -o $$@))

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

.PHONY: clean
clean:
	rm -rf $(OUTPUT_DIR)
	rm -rf $(TARGET)
