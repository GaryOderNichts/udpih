CMAKE_BUILD_DIR := build
CMAKE_SOURCE_DIR := .

.PHONY: all clean $(CMAKE_BUILD_DIR)/Makefile $(CMAKE_BUILD_DIR)/udpih.uf2

all: $(CMAKE_BUILD_DIR)/udpih.uf2

$(CMAKE_BUILD_DIR)/Makefile: $(CMAKE_SOURCE_DIR)/CMakeLists.txt
	cmake -S $(<D) -B $(@D)

$(CMAKE_BUILD_DIR)/udpih.uf2: $(CMAKE_BUILD_DIR)/Makefile
	$(MAKE) -C $(@D)

clean:
	rm -rf $(CMAKE_BUILD_DIR)
