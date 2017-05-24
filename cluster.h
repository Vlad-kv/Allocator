#ifndef CLUSTER_H
#define CLUSTER_H


class cluster {
private:
	static const int32_t MAX_RANG = 20;
	static const int32_t MIN_RANG = 6;
	static const int32_t SHIFT = 16;
	static const int32_t UNDEFINED = -MAX_RANG;
	
	char *storage;
	int32_t rang;
	int32_t size;
	
	int32_t levels[MAX_RANG + 1];
	
	int32_t &get_rang(char *ptr) {
		return *((int32_t*)ptr);
	}
	int32_t &get_prev(char *ptr) {
		return *((int32_t*)ptr + 1);
	}
	int32_t &get_next(char *ptr) {
		return *((int32_t*)ptr + 2);
	}
	
public:
	cluster(char *storage, int32_t rang)
	: storage(storage), rang(rang) {
		assert((((int64_t)storage) & 7) == 0);
		assert((MIN_RANG <= rang) && (rang <= MAX_RANG));
		
		for (int w = MIN_RANG; w <= rang; w++) {
			
		}
		
		
		get_rang(storage) = rang;
//		get_prev(storage)
		
		
	}
	
	
};

#endif // CLUSTER_H

