OUT_PUT = Run
SRC_DIR_ = src
OBJ_DIR_ = obj

OBJS = $(patsubst $(SRC_DIR_)/%.cpp, $(OBJ_DIR_)/%.o, $(wildcard $(SRC_DIR_)/*.cpp)) 	

INCLUDE_DIR = -Iinclude 
vpath %.h include
vpath %.cpp src

All: $(OBJ_DIR_) $(OBJS)
	g++  -o  $(OUT_PUT)  $(OBJS) -lpthread
$(OBJ_DIR_):
	mkdir $@
$(patsubst $(SRC_DIR_)/%.cpp, $(OBJ_DIR_)/%.o, $(wildcard $(SRC_DIR_)/*.cpp)): $(OBJ_DIR_)/%.o: $(SRC_DIR_)/%.cpp
	g++ -c  $<   $(INCLUDE_DIR) -o    $@ 

clean:
	-rm -rf $(OUT_PUT) $(OBJ_DIR_) 
	
