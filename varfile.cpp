#include "varfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <stack>

#define FRIVOLOUS " \n\t" //list of symbols parser should ingore entirely
#define CORRELATOR '='
#define TERMINATOR ';'
#define QUOTE '\"'
#define BRACKETS "{}"

#define DEBUG

int isFrivolous(char inT){//this is a private function, neccesary only for the function of this library, as such it is not neccesary to make it visible to the main code
	for(unsigned long i=0; i < sizeof(FRIVOLOUS); i++)if(inT == FRIVOLOUS[i])return 1;
	return 0;
}

unsigned long appendEntry(avf::Entry*& array, char* objname)
{
	unsigned long outsize;
	for(outsize = 0; array[outsize].name; ++outsize);

	avf::Entry* holder = new avf::Entry[++outsize+1];
	for(unsigned long i = 0; i < outsize; i++){
		holder[i].name = array[i].name;
		holder[i].value = array[i].value;
		holder[i].parent = array[i].parent ? &holder[0] + (array[i].parent - &array[0]) : NULL;
	}
	holder[outsize-1].name = objname;
	holder[outsize].name = NULL;
	delete[] array;
	array = holder;
	return outsize;
}

avf::Entry* avf::load(FILE* target){
	rewind(target);
	avf::Entry* object = new avf::Entry[1];
	object[0].name = NULL;
	//TODO: implement proper parsing to detect invalid variable names (starting with numbers, including non-aphlanumerical characters etc.)
	
	enum States{// we shall represent states using enums and switch-cases
		EE,  // end of entry
		SI, // seek for the next identifier
		ID, // identifier
		SC, // seek for the correlator
		CR, // correlator
		SV, // seek for the value
		OV, // value
		QU, // quote
		QE, // escaped character in quotes
		BL, // block (defines multi member objects)
		CM // comment
	};;
	States state = SI;

	long line=1;
	// element holder values
	unsigned long objnsize=0;
	unsigned long objvsize=0;
	char* objname;
	char* objvalue; 	
	std::stack<unsigned long> objects;	
	while(true){
		// we are entering an infinite loop, since the following code is going to be implemented as a state machine
		// the loop is going to be broken on a return call
		char inC;
		if(state)inC = fgetc(target);
		#ifdef DEBUG
		printf("input: %c, state: %d\n", inC=='\n'?'N':inC, state);
		fflush(stdout);
		#endif
		if(inC=='\n')line++;//keep track of lines, will be used to display errors
		if(inC==EOF){
			if(state==SI)return object;
			else{
				delete[] object;
				object = new avf::Entry[1];
				object[0].name = new char[1];
				object[0].name[0]=0;
				object[0].value = new char[200];
				sprintf((char*)object[0].value, "line %ld: unexpected end-of-file\n", line);
				object[0].value[199]='\0';
				return object;
			}
		}
		switch(state){
			case SI:
				if(isFrivolous(inC))break;
				else if(inC == BRACKETS[1]){
					objects.pop();
					break;
				}
				else {
					state = ID;
				}// otherwise assume we reached an identifier
			
			case ID:
				if(objnsize==2){
					//TODO: following condition is impossible to meet. fix that
					if(objname[0] == '/' && objname[1] == '/'){// if the input appears to be a start of comment
						objnsize=0;
						delete[] objname;
						objname=NULL;
						state = CM;// mark it as such
						break;
					}
				}
				if(inC >= 'A' && inC <= 'Z' || inC >= 'a' && inC <= 'z' || inC >= '0' && inC <= '9'){// if its an alphanumerical character
					char* holder = new char[++objnsize+1];// add it to the checking stream
					//holder is an additional character long in order to hold null terminator
					for(unsigned long i = 0; i < objnsize-1;i++)holder[i]=objname[i];
					holder[objnsize-1]=inC;
					if(objnsize > 1) delete[] objname;
					objname=holder;//dirty dirty append
					break;
				}

				else if(isFrivolous(inC)){//end of identifier
					objname[objnsize]='\0';
					state=CR;//assume we are looking for a correlator
				}

				else {
					delete[] object;
					object = new avf::Entry[1];
					object[0].name = new char[1];
					object[0].name[0]=0;
					object[0].value = new char[200];
					sprintf((char*)object[0].value, "line %ld: invalid character in variable declaration\n", line);
					object[0].value[199]='\0';
					return object;
				}
			case CR:
				if(inC == CORRELATOR){
					state = SV;
					break;
				}
				
				else if(inC == BRACKETS[0]){
					unsigned long outsize = appendEntry(object, objname);
					objname = NULL;
					objnsize = 0;
					//append the scanned values to output list
					//NOTE: code above is a modified copy-paste from the EE state, mostly because we just need to create a new entry
					if(!objects.empty())object[outsize-1].parent=&object[objects.top()]; // if theres something in the stack, put it in as a parent
					else object[outsize-1].parent = NULL;
					objects.push(outsize-1);
					state = SI; // rinse and repeat
					break;
				}

				else if(isFrivolous(inC))break;
				
				else {
					delete[] object;
					object = new avf::Entry[1];
					object[0].name = new char[1];
					object[0].name[0]=0;
					object[0].value =  new char[200];
					sprintf((char*)object[0].value, "line %ld: expected \"%c\", got \"%c\"\n", line, CORRELATOR, inC);
					object[0].value[199]='\0';
					return object;
				}
			case SV:
				if(isFrivolous(inC))break; // ignore anything that could not be an object
				else if(inC == '\"'){
					state = QU;
					break;
				}	
				else state = OV;

			case OV:
				if(inC == TERMINATOR){
					objvalue [objvsize]='\0';
					state=EE;
					break;
				}
				else if(inC >= '0' && inC <= '9'){ // if our value is numeric
					char* holder = new char[++objvsize+1]; // apppend it
					for(unsigned long i = 0; i < objvsize-1;i++)holder[i]=objvalue[i];
					holder[objvsize-1]=inC;
					if(objvsize > 1) delete[] objvalue;
					objvalue=holder;
					break;
				}

				else if(isFrivolous(inC)){
					break;
				}
				else{
					delete[] object;
					object = new avf::Entry[1];
					object[0].name = new char[1];
					object[0].name[0] = 0;
					object[0].value =  new char[200];
					sprintf((char*)object[0].value, "line %ld: unexpected value for %s \n", line, objname);
					object[0].value[199]='\0';
					return object;
				}
			case QU:
				if(inC == QUOTE){ // if second quotation mark is found
					state = OV; // switch to end state: we know nothing more is here
					break; // TODO: wacky. could cause bugs. implement seek to end state maybe???
				}
				if(inC == '\\'){ // if escape sign is encountered
					state = QE;
					break;
				}
				else {
					char* holder = new char[++objvsize+1]; // apppend it
					for(unsigned long i = 0; i < objvsize-1;i++)holder[i]=objvalue[i];
					holder[objvsize-1]=inC;
					if(objvsize > 1) delete[] objvalue;
					objvalue=holder;
					break;
				}
			case QE:{
				char* holder = new char[++objvsize+1]; // apppend it
				for(unsigned long i = 0; i < objvsize-1;i++)holder[i]=objvalue[i];
				holder[objvsize-1]=inC;
				if(objvsize > 1) delete[] objvalue;
				objvalue=holder;
				state = QU;
				break;
			}
			case EE:{
					unsigned long outsize = appendEntry(object, objname);
					object[outsize-1].value = objvalue;
					if(!objects.empty())object[outsize-1].parent = &object[objects.top()];
					else object[outsize-1].parent = NULL;
					object[outsize].name = NULL;
					objname = NULL;
					objvalue = NULL;
					objnsize = 0;
					objvsize = 0;
					//append the scanned values to output list
					state = SI; // rinse and repeat
					break;
				}
		}
	}
}

unsigned long avf::write(FILE* target, avf::Entry* values){
	unsigned long outlen = 0; // tracks the number of characters written

	unsigned long Elen;
	for(Elen = 0; values[Elen].name; Elen++);
	#ifdef DEBUG
	printf("%ld\n", Elen);
	#endif
	avf::Entry** holder = new avf::Entry*[Elen+1];//create references to the input array. they will be removed as they are being written
	for(unsigned long i = 0; i < Elen; i++)holder[i] = &values[i];
	
	std::stack<avf::Entry*> layers;
	unsigned long i = 0;
	while(i < Elen || !layers.empty()){
		#ifdef DEBUG
		printf("elem: %ld\n", i);
		#endif
		if(holder[i]){
			/*#ifdef DEBUG
			if(!layers.empty()){
				printf("%ld\n%ld\n", holder[i]->parent, layers.top());
			}
			#endif*/
			printf("0x%lx\n", holder[i]);
			fflush(stdout);
			if(!holder[i]->value){
				#ifdef DEBUG
				printf("object definition: 0x%lx\n", holder[i]);
				#endif
				char* tabs = new char[layers.size()+1];
				for(unsigned long j = 0; j < layers.size(); j++)tabs[j]='\t';
				tabs[layers.size()]='\0';
				outlen += fprintf(target, "%s%s %c\n", tabs, holder[i]->name, BRACKETS[0]);
				
				layers.push(holder[i]);
				printf("0x%lx, 0x%lx\n", layers.top(), holder[i]);
				holder[i]=NULL;
				i = 0; //restart counting
			}
			else if(layers.empty() && !holder[i]->parent){//WHY IS THIS NOT WORKING???
				outlen += fprintf(target, "%s %c %s%c\n", holder[i]->name, CORRELATOR, holder[i]->value, TERMINATOR);
				holder[i]=NULL;
				i++;
			}
			else if( holder[i]->parent == layers.top() ){
				printf("t1\n");
				char* tabs = new char[layers.size()+1];
				for(unsigned long j = 0; j < layers.size(); j++)tabs[j]='\t';
				tabs[layers.size()]='\0';
				outlen += fprintf(target, "%s%s %c %s%c\n",tabs, holder[i]->name, CORRELATOR, holder[i]->value, TERMINATOR);
				holder[i]=NULL;
				i++;
			}
			else {
			    printf("did nothing\n");
			    i++;
			}
			if( i == Elen - 1 && !layers.empty() ){
				printf("layers.pop\n");
				layers.pop();
				char* tabs = new char[layers.size()+1];
				for(unsigned long j = 0; j < layers.size(); j++)tabs[j]='\t';
				tabs[layers.size()]='\0';
				outlen += fprintf(target, "%s%c\n", tabs, BRACKETS[1]);
				i=0;
			}
		}
		else {
			i++;
			printf("already processed. skipping...\n");
		}
		fflush(target);
	}
	return outlen;
}

char* avf::get(FILE* target, char* key){
	return 0;
}
int avf::put(FILE* target, char* key, char* value){
	return 0;
}
//TODO: write these functions
