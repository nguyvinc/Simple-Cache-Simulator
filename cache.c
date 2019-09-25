#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

struct set{
	int index;
	struct block* blocks;
};

struct block{
	int dirty;
	int valid;
	int used;
	unsigned long tag;
};

void read_config_file(FILE*, int*, char**);
int check_line(char*);
void read_trace_file(FILE*, struct set*, int[8], char*);
void store_instruction(struct set*, int[8], int, int, unsigned long, int, int, FILE*, char*, int[4]);
void load_instruction(struct set*, int[8], int, int, unsigned long, int, int, FILE*, char*, int[4]);
void modify_instruction(struct set*, int[8], int, int, unsigned long, int, int, FILE*, char*, int[4]);


void free_mem(struct set*, int);
void print_cache(struct set*, int, int);

int main(int argc, char* argv[]){
	if(argc != 3){	//3 arguments must be provided
		printf("./%s [cache_config_file] [trace_file_name]\n", argv[0]);
	}
	else{			//If 3 arguments are provided
		FILE *fptr = fopen(argv[1], "r");	//Open the config file listed in the second argument

		if(fptr == NULL){					//If the file was not found
			printf("Error: file '%s' not found\n", argv[1]);	//Print an error
			exit(1);											//Exit
		}
		else{								//Else, the file was found
			int config[8];						//Array to store the 7 configuration values
			read_config_file(fptr, config, argv);	//Read the config file and fill out the config information
			fclose(fptr);

			/*
			num_sets = config[0];
			block_size = config[1];
			associativity = config[2];
			replacement = config[3];		//0 = random, 1 = LRU
			write = config[4];				//0 = write-through, 1 = write-back
			cache_cycle = config[5];
			memory_cycle = config[6];
			cache_coherence = config[7];	//NOT USED IN THIS IMPLEMENTATION
			*/

			struct set* cache = malloc(sizeof(struct set) * config[0]);			//Create cache with right number of sets
			int i, j;
			for(i=0; i<config[0]; i++){											//Initialize each set
				cache[i].index = i;
				cache[i].blocks = malloc(sizeof(struct block) * config[2]);	//Allocate each set with blocks
				for(j=0; j<config[2]; j++){									//Initialize each block
					cache[i].blocks[j].dirty = 0;
					cache[i].blocks[j].used = 0;
					cache[i].blocks[j].valid = 0;
					cache[i].blocks[j].tag = 0;
				}
			}

			//Begin trace file read
			fptr = fopen(argv[2], "r");
			if(fptr == NULL){
				printf("Error: file '%s' not found\n", argv[2]);	//Print an error
				free_mem(cache, config[0]);
				exit(1);											//Exit
			}
			else{
				read_trace_file(fptr, cache, config, argv[2]);
				fclose(fptr);
			}

			//print_cache(cache, config[0], config[2]);
			free_mem(cache, config[0]);
		}
	}
	return 0;
}


void read_config_file(FILE* fptr, int *config, char* argv[]){
	char* line = NULL;
	size_t length = 0;
	ssize_t read;
	int i = 0, valid = 0;
	
	while((read = getline(&line, &length, fptr)) != -1){	//Read each line in the file
		valid = check_line(line);							//Check the line that was read
		if(valid == 0){
			printf("Error: first eight lines in config file '%s' must be integers\n", argv[1]);
			exit(1);
		}
		if(i < 8){						//If less than 8 lines were read so far
			config[i] = atoi(line);		//Convert the current line into an integer
			i++;						//Increment array index
		}
	}
	if(i < 8){	//If 8 lines were not read, file is not valid
		printf("Error: first eight lines in config file '%s' must be integers\n", argv[1]);
		exit(1);
	}
	free(line);
}


int check_line(char* line){
	int i, space = 0;	//Keep track if there is a space in a line
	for(i = 0; i < strlen(line); i++){	//Check all characters in the line
		if(!(line[i] >= '0' && line[i] <= '9')){	//If the character isn't a number
			if(line[i] == ' ' || line[i] == '\0' || line[i] == '\n'){	//If the character is a space, null, or line break
				space = 1;							//There is a space
			}
			else									
				return 0;	//The line is invalid
		}
		else if (space == 1)	//If the character is a number, but a space was detected earlier
			return 0;		//The line is invalid
	}
	return 1;				//If function gets here, the line is valid
}


void read_trace_file(FILE* fptr, struct set* cache, int config[8], char* trace){
	srand(time(NULL));
	char* line = NULL;
	size_t length = 0;
	ssize_t read;

	char* outfile = trace;		//Copy the name of the trace file
	strcat(outfile, ".out");	//Append .out to the name
	FILE* out = fopen(outfile, "w");	//Create the output file

	int summary[4] = {0, 0, 0, 0};
	//int total_cycles = 0, hits = 0, misses = 0, evictions = 0;	//Keep track of overall statistics
	int hex[6] = {10, 11, 12, 13, 14, 15};	//Used to convert a-e characters to decimal
	int i, num, cache_index, used = 1;
	unsigned long block_address, tag;

	while(read = getline(&line, &length, fptr) != -1){	//Read each line in the provided trace file
		if(!(line[0] == '=' || line[0] == 'I')){	//Do nothing with comments or instruction load commands
			char* copy = malloc(sizeof(char) * strlen(line)+1);
			strcpy(copy, line);

			char* tok = strtok(copy, " ,'\n'");	//Instruction
			tok = strtok(NULL, " ,'\n'");		//Address
			char* address = malloc(sizeof(char) * strlen(tok)+1);
			strcpy(address, tok);
			tok = strtok(NULL, " ,'\n'");		//Size
			tok = strtok(NULL, " ,'\n'");		//Empty the strtok

			//Address calculations, convert hex to decimal
			unsigned long decimal=0, j=0;
			for(i = strlen(address)-1; i >= 0; i--){
				if(address[i] >= 'a' && address[i] <= 'f')
					num = hex[((int) address[i])-97];	//97 is ASCII for 'a'
				else if(address[i] >= 'A' && address[i] <= 'F')
					num = hex[((int) address[i])-65];	//65 is ASCII for 'A'
				else if (address[i] >= '0' && address[i] <= '9')
					num = ((int) address[i])- 48;	//48 is ASCII for '0'
				decimal += num * pow(16, j);	//Decimal represents byte address
				j++;
			}
			
			block_address = decimal / config[1];		//Block address = byte address / bytes per block (integer division, drop remainder)
			cache_index = block_address % config[0];		//Cache index = block address % number of indices or sets
			tag = block_address / config[0];				//Tag = block address / number of indices or sets (integer division, drop remainder)
			
			int found = -1, full = 1;
			for(i = 0; i < config[2]; i++){
				if(cache[cache_index].blocks[i].tag == tag && cache[cache_index].blocks[i].valid == 1)	//If a block has the same tag and is valid, block is found in cache
					found = i;
				if(cache[cache_index].blocks[i].valid == 0)	//If an invalid block is found, cache is not full
					full = 0;
			}

			if(line[1] == 'S')		//If a store operation
				store_instruction(cache, config, found, used, tag, cache_index, full, out, line, summary);
			else if(line[1] == 'L')	//If a load operation
				load_instruction(cache, config, found, used, tag, cache_index, full, out, line, summary);
			else if(line[1] == 'M'){	//If a modify operation
				modify_instruction(cache, config, found, used, tag, cache_index, full, out, line, summary);
				//fprintf(out, "%s", line);
			}
			used++;					//Counter to keep track of when a block was used
			free(copy);
			free(address);
		}
	}
	fprintf(out, "Hits:%d Misses:%d Evictions:%d\nCycles:%d", summary[1], summary[2], summary[3], summary[0]);
	fclose(out);
	free(line);
}


void store_instruction(struct set* cache, int config[8], int found, int used, unsigned long tag, int cache_index, int full, FILE* out, char* line, int summary[4]){
	char *impact;
	int cycles = 0, i;
	if(config[4] == 0)	//0 = write-through
		cycles += config[5] + config[6];		//Had to write to cache and main memory
	else				//1 = write-back
		cycles += config[5];					//Only had to write to cache
		
	if(found != -1){	//If the block was found in the cache already, hit
		(summary[1])++;
		cache[cache_index].blocks[found].used = used;
		cache[cache_index].blocks[found].dirty = 1;		//Data store to cache, block is now dirty
		impact = "hit";
	}
	else{				//Else it was a miss
		(summary[2])++;
		if(full == 0){	//If index is not full, no eviction
			for(i = 0; i < config[2]; i++){
				if(cache[cache_index].blocks[i].valid == 0){	//Add the block to the cache
					found = i;
					break;
				}
			}
			impact = "miss";
		}
		else{			//If index is full, there is an eviction
			(summary[3])++;
			if(config[3] == 0)	//0 = random replacement
				found = rand() % config[2];					//Pick a random block index between 0 and associativity-1
			else{					//1 = LRU replacement
				int LRU = cache[cache_index].blocks[0].used;	//Set the LRU value to the first blocks used value
				found = 0;										//Set the block index to the first one
				for(i = 1; i < config[2]; i++){				//Compare all the blocks in the set
					if(cache[cache_index].blocks[i].used < LRU){//If one was used less recently
						LRU = cache[cache_index].blocks[i].used;//Set the LRU value to the current block's used value
						found = i;								//Update the block index to match the current block
					}
				}	
			}
			if(config[4] == 1 && cache[cache_index].blocks[found].dirty == 1){ 	//If write-back and evicted block is dirty
				impact = "miss dirty_eviction";
				cycles += config[6];	//Had to access main memory again for write-back
			}
			else
				impact = "miss eviction";
		}
		cache[cache_index].blocks[found].valid = 1;		//Replace the selected block on miss/evict
		cache[cache_index].blocks[found].used = used;
		cache[cache_index].blocks[found].tag = tag;
		cache[cache_index].blocks[found].dirty = 1;		//Block is dirty since data is written to the cache, only looked at in write-back mode
	}
	if(line[strlen(line)-1] == '\n')
		line[strlen(line)-1] = '\0';
	fprintf(out, "%s %d %s\n", line, cycles, impact);
	summary[0] += cycles;	//Count total number of cycles overall
}


void load_instruction(struct set* cache, int config[8], int found, int used, unsigned long tag, int cache_index, int full, FILE* out, char* line, int summary[4]){
	char *impact;
	int cycles = 0, i;
	if(found != -1){	//If the block was found in the cache already, hit
		(summary[1])++;
		cycles += config[5];					//Only had to access cache
		cache[cache_index].blocks[found].used = used;
		impact = "hit";
	}
	else{				//Else it was a miss
		(summary[2])++;
		cycles += config[5] + config[6];	//Had to access cache and main memory
		if(full == 0){	//If index is not full, no eviction
			for(i = 0; i < config[2]; i++){
				if(cache[cache_index].blocks[i].valid == 0){	//Add the block to the cache
					found = i;
					break;
				}
			}
			impact = "miss";
		}
		else{			//If index is full, there is an eviction
			(summary[3])++;
			if(config[3] == 0)	//0 = random replacement
				found = rand() % config[2];					//Pick a random block index between 0 and associativity-1
			else{					//1 = LRU replacement
				int LRU = cache[cache_index].blocks[0].used;	//Set the LRU value to the first block's used value
				found = 0;										//Set the block index to the first one
				for(i = 1; i < config[2]; i++){				//Compare all the blocks in the set
					if(cache[cache_index].blocks[i].used < LRU){//If the current block was used less recently
						LRU = cache[cache_index].blocks[i].used;//Set the LRU value to the current block's used value
						found = i;								//Update the block index to match the current block
					}
				}	
			}
			if(config[4] == 1 && cache[cache_index].blocks[found].dirty == 1){ 	//If write-back and block is dirty
				impact = "miss dirty_eviction";
				cycles += config[6];	//Had to access main memory again for write-back
			}
			else
				impact = "miss eviction";
		}
		cache[cache_index].blocks[found].valid = 1;		//Replace the selected block on miss/evict
		cache[cache_index].blocks[found].used = used;
		cache[cache_index].blocks[found].tag = tag;
		cache[cache_index].blocks[found].dirty = 0;		//Only looked at in write-back mode, loaded in block is not dirty
	}
	if(line[strlen(line)-1] == '\n')
		line[strlen(line)-1] = '\0';
	fprintf(out, "%s %d %s\n", line, cycles, impact);
	summary[0] += cycles;	//Count total number of cycles overall
}

void modify_instruction(struct set* cache, int config[8], int found, int used, unsigned long tag, int cache_index, int full, FILE* out, char* line, int summary[4]){
	char *impact;
	int cycles = 0, i;
	if(found != -1){	//If the block to be modified was found in the cache
		(summary[1]) += 2;	//Hit when block is already loaded in cache, and hit when stored into memory
		if(config[4] == 0)
			cycles += (2*config[5]) + config[6];	//Scanned cache for block, accessed cache to modify, then stored to memory
		else
			cycles += (2*config[5]);				//Scanned cache for block, accessed cache to modify
		cache[cache_index].blocks[found].used = used;
		impact = "hit hit";
	}
	else{				//Else, the block to be modified is not in the cache
		(summary[2])++;	//Add a miss
		(summary[1])++;	//Add a hit
		if(config[4] == 0)
			cycles += (2*config[5]) + (2*config[6]);	//Had to access cache and main memory to load proper block in, then access cache to modify block, then store to memory
		else
			cycles += (2*config[5] + config[6]);		//Had to access cache and main memory to load proper block in, then access cache to modify block

		if(full == 0){	//If the index is not full, no eviction
			for(i = 0; i < config[2]; i++){
				if(cache[cache_index].blocks[i].valid == 0){	//Add the block to the cache
					found = i;
					break;
				}
			}
			impact = "miss hit";
		}
		else{			//If the index is full, there is an eviction
			(summary[3])++;	//Add an eviction
			if(config[3] == 0)	//0 = random replacement
				found = rand() % config[2];
			else{				//1 = LRU replacement
				int LRU = cache[cache_index].blocks[0].used;	//Set the LRU value to the first block's used value
				found = 0;
				for(i = 1; i < config[2]; i++){				//Compare all the blocks in the set
					if(cache[cache_index].blocks[i].used < LRU){	//If the current block was used less recently
						LRU = cache[cache_index].blocks[i].used;	//Set the LRU value to the current block's used value
						found = i;									//Update the block index to match the current block
					}
				}
			}
			if(config[4] == 1 && cache[cache_index].blocks[found].dirty == 1){	//If write-back and block is dirty
				impact = "miss dirty_eviction hit";
				cycles += config[6];	//Had to access main memory again for write-back
			}
			else
				impact = "miss eviction hit";
		}
		cache[cache_index].blocks[found].valid = 1;		//Replace the selected block on miss/evict
		cache[cache_index].blocks[found].used = used;
		cache[cache_index].blocks[found].tag = tag;
		cache[cache_index].blocks[found].dirty = 0;		//Only looked at in write-back mode, data is stored, so no longer dirty
	}
	if(line[strlen(line)-1] == '\n')
		line[strlen(line)-1] = '\0';
	fprintf(out, "%s %d %s\n", line, cycles, impact);
	//fprintf(out, "%s %d %s Tag: %ld\n", line, cycles, impact, tag);
	summary[0] += cycles;	//Count total number of cycles overall
}



void free_mem(struct set* cache, int num_sets){
	int i;
	for(i=0; i<num_sets; i++)
		free(cache[i].blocks);
	free(cache);
}


void print_cache(struct set* cache, int num_sets, int associativity){
	int i, j;
	printf("\nCache:\n");
	for(i=0; i<num_sets; i++){
		printf("Index %d | ", cache[i].index);
		for(j=0; j<associativity; j++){
			printf("Block %d: used %d, valid %d, tag %d | ", j, cache[i].blocks[j].used, cache[i].blocks[j].valid, cache[i].blocks[j].tag);
		}
		printf("\n");
	}
}
