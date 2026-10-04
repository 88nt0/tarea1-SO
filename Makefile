CC = g++
CFLAGS = -Wall -Wextra -std=c++17 -lpthread
TARGET = planificador
SRC = src/main.cpp

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
