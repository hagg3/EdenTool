
//Basic File Structure is
//Header
	//ChunkColumn (x,z)
			//Chunk
			//Chunk
			//Chunk
			//Chunk
	//ChunkColumn
			//Chunk
			//Chunk
			//Chunk
			//Chunk
//Index directory of where each chunk column is located on the map(x,z) and the offset in bytes into the file 


#pragma once
#define FILE_VERSION 4

//File block data is subdivided into CHUNK_SIZE x CHUNK_SIZE x CHUNK_SIZE parts
#define CHUNK_SIZE 16    

//how many chunks to read around the players position
#define T_READ_RADIUS 8   
#define T_SIZE (T_READ_RADIUS*2*CHUNK_SIZE)
#define T_HEIGHT 64


//program expects blockarray and colorarray to be declared and allocated by the parent file
#define GBLOCK(x,z,y)  blockarray[((x)*(T_SIZE*T_HEIGHT) + ((z)*T_HEIGHT) + (y))]
#define GCOLOR(x,z,y)  colorarray[((x)*(T_SIZE*T_HEIGHT) + ((z)*T_HEIGHT) + (y))]

typedef signed char block8;
typedef unsigned char color8;





typedef struct {
	float    x;
	float    y;
	float    z;
}Vector;


typedef struct {
	int level_seed;
	Vector pos;  //player position
	Vector home;
	float yaw; //initial camera yaw
	unsigned long long directory_offset;  //location of chunk index
	char name[50];

	//below here is post 1.1.1 stuff
	int version;
	char hash[36]; //verification hash of shared world preview image
	unsigned char skycolors[16];
	int goldencubes;
	char reserved[100 - sizeof(int) - 36 - 16 - sizeof(int)];	
}WorldFileHeader;

typedef struct {
	int x, z;
	unsigned long long chunk_offset;
}ColumnIndex;



class EdenFileLoader {
public:
	void loadWorld(char* name);
	// New: Convert entire Eden world to Minecraft (1.12 Anvil) at output directory
	void convertToMinecraft(const char* edenPath, const char* outputWorldDir);
private:
	bool readColumn(int cx, int cz);
	void readDirectory();
};





