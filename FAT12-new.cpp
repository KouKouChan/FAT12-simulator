#include<iostream>
#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<memory.h>
#include<ctime>
using namespace std;
#define BLOCKSIZE 512	//һ��������С 
#define FILESIZE 1474560  // ������̿ռ��С
typedef unsigned char u8;	//1�ֽ�
typedef unsigned short u16;	//2�ֽ�
typedef unsigned int u32;	//4�ֽ�

struct Head {
	char BS_jmpBOOT[3];
	char BS_OEMName[8];
};

struct BPB {
	u16  BPB_BytsPerSec;    //ÿ�����ֽ���
	u8 BPB_SecPerClus;      //ÿ��������
	u16  BPB_RsvdSecCnt;    //Boot��¼ռ�õ�������
	u8 BPB_NumFATs;	        //FAT�����
	u16  BPB_RootEntCnt;    //��Ŀ¼����ļ���
	u16  BPB_TotSec16;      //��������
	u8 BPB_Media;           //����������
	u16  BPB_FATSz16;	   //FAT������
	u16  BPB_SecPerTrk;    //ÿ�ŵ�������
	u16  BPB_NumHeads;     //��ͷ��
	u32  BPB_HiddSec;      //����������
	u32  BPB_TotSec32;	   //���BPB_FATSz16Ϊ0����ֵΪFAT������
};

struct BS {
	u8 BS_DrvNum;              //INT 13H����������
	u8 BS_Reserved1;           //������δʹ��
	u8 BS_BootSig;             //��չ�������(29h)
	u32 BS_VolID;			   //�����к�
	char BS_VolLab[11];          //���
	char BS_FileSysType[8];    //�ļ�ϵͳ����
};

//��Ŀ¼��Ŀ
struct RootEntry {
	char DIR_Name[11];
	u8   DIR_Attr;		//�ļ�����
	char reserved[10];	//�����ռ� 
	u16  DIR_WrtTime;	//�ļ�д��ʱ�� 
	u16  DIR_WrtDate;
	u16  DIR_FstClus;	//��ʼ�غ�
	u32  DIR_FileSize;
};

struct fat_c {
	u8 c;
};

u8* VirtulMemory;	//�����ڴ� 
int root_base;		//Ŀ¼���׵�ַ 
int data_base;		//�������׵�ַ 
int fat_base;		//FAT���׵�ַ 
int maxfats;		//FAT���дص������Ŀ 
BPB *bpb_ptr;
RootEntry* root;
RootEntry* dir;
int RsvdSecCnt;
int NumFATs;
int BytsPerSec;
int RootEntCnt;
BS* bs_ptr;
char openlist[1024];	//�洢��·�� 
char *command;			//�洢������������ 
int locationroot;		//��ǰ��Ŀ¼�ĵ�ַ 
int maxrootnum;			//��ǰ��Ŀ¼������ļ��� 
char writetemp[4096];	//�洢�û�д���ļ������� 
char opendir[1024];

inline int read() {
	int s=0,f=1;
	char c=getchar();
	while(c<'0'||c>'9') {
		if(c=='-')f=-1;
		c=getchar();
	}
	while(c>='0'&&c<='9') {
		s=s*10+c-48;
		c=getchar();
	}
	return s*f;
}

bool strcomp(const char* a,const char* b); //�Զ�����ַ����Ƚ� 
u16 FATread(u16 fatnum,bool Is);			//��ȡFAT���е�һ���� 
void FATwrite(u16 fatnum,u16 src);			//��FAT��д��һ���� 
u16 FATfindend(u16 fatnum);					//Ѱ���ļ���FAT���е�ĩβ�� 
void Init(FILE* f);							//��ʼ���ļ� 
void menu();								//�˵� 
void showfile();							//��ʾ��ǰĿ¼�µ��ļ� 
void newfile(const char *ch,bool IsnewFile);				//�½��ļ���Ŀ¼���� IsnewFile�ж� 
void deletefile(const char*ch,bool Isroot,int location);	//ɾ���ļ��� �� Isroot�ж��Ƿ���Ŀ¼�� 
int findfile(const char *ch,u8 Isfile);						//Ѱ���ļ� 
void deleteroot(int loaction);								//ɾ��Ŀ¼ 
void writefile(const char*ch);								//���ļ�д������������ 
void readfile(const char*ch);								//��ȡ�ļ� 
void InitVal();												//��ʼ������ 
void cd(const char*ch);										//�л�Ŀ¼ 
void deldir(const char*ch);									//ɾ��Ŀ¼������ 
void readchar(int *check,int *num,char **c,RootEntry* temp_root);	//һ���ֽ�һ���ֽڶ�ȡ�ļ����� 
int writedata(int location,const char* str,int last,int start);		//д������ 
void help();								//������Ϣ 
void info();								//������Ϣ 
bool checkname(const char*ch);				//����ļ��Ƿ����� 

int main() {
	VirtulMemory = (u8 *)malloc(FILESIZE);  //����������̿ռ�
	FILE* fp=fopen("floopy.img","rb");
	if(fp==NULL) Init(fp);					//�ļ��������򴴽�һ�� 
	else {
		fread(VirtulMemory,FILESIZE,1,fp);
		fclose(fp);
		bpb_ptr=(BPB *)(VirtulMemory+11);
		bs_ptr=(BS*)(VirtulMemory+36);
	}
	InitVal();								//��ʼ������ 
	locationroot=root_base;
	maxrootnum=RootEntCnt-1;
	menu();
	return 0;
}

//�Զ����ַ����Ƚ�
bool strcomp(const char* a,const char* b) {
	while(*a == *b) {
		if( *a == '\0') return true;
		++a,++b;
	};
	return false;
}

void creatFile(const char *ch) {
	FILE* fp=fopen("floopy.img","wb");
	memset(VirtulMemory,0,FILESIZE);
	Head *header;
	header=(Head *)VirtulMemory;
	strcpy(header->BS_jmpBOOT,"12");
	strcpy(header->BS_OEMName,"LveYYn");
	bpb_ptr=(BPB*)(VirtulMemory+11);
	bpb_ptr->BPB_BytsPerSec=0X200;
	bpb_ptr->BPB_FATSz16=0x9;
	bpb_ptr->BPB_HiddSec=0;
	bpb_ptr->BPB_NumHeads=0x2;
	bpb_ptr->BPB_RootEntCnt=0xE0;
	bpb_ptr->BPB_RsvdSecCnt=0x1;
	bpb_ptr->BPB_SecPerTrk=0x12;
	bpb_ptr->BPB_TotSec16=0xB40;
	bpb_ptr->BPB_TotSec32=0;
	bpb_ptr->BPB_Media=0X1;
	bpb_ptr->BPB_NumFATs=0X2;
	bpb_ptr->BPB_SecPerClus=0X1;
	bs_ptr=(BS*)(VirtulMemory+36);
//	printf("�����������:");
//	scanf("%s",&bs_ptr->BS_VolLab);
	strcpy(bs_ptr->BS_VolLab,ch);
	strcpy(bs_ptr->BS_FileSysType,"FAT12  ");
	bs_ptr->BS_FileSysType[7]=' ';
	bs_ptr->BS_BootSig=0X29;
	fat_c* charc=(fat_c*)(VirtulMemory+512);
	charc->c=0XF0;
	charc++;
	charc->c=0XFF;
	charc++;
	charc->c=0XFF;
	charc=(fat_c*)(VirtulMemory+512+bpb_ptr->BPB_FATSz16*bpb_ptr->BPB_BytsPerSec);
	charc->c=0XF0;
	charc++;
	charc->c=0XFF;
	charc++;
	charc->c=0XFF;
	RootEntry* root=(RootEntry*)(VirtulMemory+9728);
	time_t now=time(NULL);
	struct tm *nowtime = localtime(&now);
	strcpy(root->DIR_Name,bs_ptr->BS_VolLab);
	root->DIR_Attr=0x28;
	root->DIR_WrtTime=nowtime->tm_hour * 2048 + nowtime->tm_min * 32 + nowtime->tm_sec / 2;
	root->DIR_WrtDate=(nowtime->tm_year - 80) * 512 + (nowtime->tm_mon + 1) * 32 + nowtime->tm_mday;
	root->DIR_FstClus=0;
	root->DIR_FileSize=0;
	charc=(fat_c*)(VirtulMemory+17407);
	int i;
	for(i=1; i<=FILESIZE-16896; i++,charc++) charc->c=0XF6;   		//�����������ֽ�ȫ��תΪ0XF6 
	fwrite(VirtulMemory, FILESIZE, 1, fp);
	fclose(fp);
}
void InitVal() {
	RsvdSecCnt=bpb_ptr->BPB_RsvdSecCnt;
	NumFATs=bpb_ptr->BPB_NumFATs;
	BytsPerSec=bpb_ptr->BPB_BytsPerSec;
	RootEntCnt=bpb_ptr->BPB_RootEntCnt;
	maxfats=bpb_ptr->BPB_FATSz16*BytsPerSec*3/2;
	root_base=(RsvdSecCnt+NumFATs*bpb_ptr->BPB_FATSz16)*BytsPerSec;
	data_base=root_base+RootEntCnt*32+BytsPerSec-1;
	fat_base=RsvdSecCnt * BytsPerSec;
	dir = (RootEntry*)(VirtulMemory+root_base);
	strcpy(opendir,"Disk:\\");
	locationroot=root_base;
}
void Init(FILE *f) {
	fprintf(stderr,"FAT12 file is not exist,trying to creat one...\n");
	fclose(f);
	printf("Please enter a disk name��");
	char name[11];
	scanf("%s",name);
	getchar();
	creatFile(name);
	system("cls");
}
void info() {
	printf("���:%s\n",bs_ptr->BS_VolLab);
	printf("�ļ�ϵͳ����:%s\n",bs_ptr->BS_FileSysType);
	printf("���̴�С:%d�ֽ�\n",FILESIZE);
	printf("����OEM:LveYYn\n");
	printf("ÿ�����ֽ���:%d\n",BytsPerSec);
	printf("ÿ��������:%d\n",bpb_ptr->BPB_SecPerClus);
	printf("FAT�����:2\n");
	printf("Boot��¼ռ�õ�������:%d\n",RsvdSecCnt);
	printf("��Ŀ¼����ļ���:%d\n",RootEntCnt);
	printf("FAT������:%d\n",bpb_ptr->BPB_FATSz16);
	printf("FAT����ʼλ��:%d�ֽ�\n",fat_base);
	printf("��Ŀ¼��ʼλ��:%d�ֽ�\n",root_base);
	printf("��������ʼλ��:%d�ֽ�\n",data_base);
}
void help() {
	printf("*********************************************************************\n");
	printf("usage:\n");
	printf("	format <newname>:   			��ʽ������\n");
	printf("	ls:   					��������ļ�\n");
	printf("	cd <dirname>:   			�л�Ŀ¼(..����)\n");
	printf("	mkdir <dirname>:   			�½�Ŀ¼\n");
	printf("	deldir <dirname>:   			ɾ��Ŀ¼\n");
	printf("	creat <filename>:   			�½��ļ�\n");
	printf("	del <filename>:   			ɾ���ļ�\n");
	printf("	write <filename>:   			д���ļ�\n");
	printf("	read <filename>:   			��ȡ�ļ�\n");
	printf("	cls:   					����\n");
	printf("	info:   				��ʾ������Ϣ\n");
	printf("	help:   				������������\n");
	printf("	exit:   				���沢�˳�\n");
	printf("*********************************************************************\n");
}
void menu() {
	printf("Welcome to my fat12 simulator system beta edition.\n");
	printf("You can enter 'help' for more information.\n");
	while(1) {
		printf("%s>",opendir);
		gets(openlist);
		if(strlen(openlist)==0) continue;				//����������Ч�� 
		command=strtok(openlist," ");					//������ֽ� 
		if(command==NULL) continue;						//����������Ч��
		if(strcomp(command,"format")) {
			command=strtok(NULL," ");					//������ֽ� 
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//����������Ч��
			if(command=='\0'||strlen(command)>=11) {
				printf("Wrong Command��\n");
				continue;
			}
			if(strlen(command)>=11) {	printf("Name is too long��\n");continue;}
			creatFile(command);
			InitVal();
			system("pause");
			system("cls");
			printf("Welcome to my fat12 simulator system beta edition.\n");
			printf("You can enter 'help' for more information.\n");
		} else if(strcomp(command,"ls")) showfile();
		else if(strcomp(command,"info")) info();
		else if(strcomp(command,"help")) help();
		else if(strcomp(command,"cls")) {
			system("cls");
			printf("Welcome to my fat12 simulator system beta edition.\n");
			printf("You can enter 'help' for more information.\n");
		}
		else if(strcomp(command,"cd")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//����������Ч��
			if(strlen(command)>=11) {	printf("Name is too long��\n");continue;}
			cd(command);
		} else if(strcomp(command,"mkdir")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//����������Ч��
			if(strlen(command)>=11) {	printf("Name is too long��\n");continue;}
			if(checkname(command)){printf("Name is invalid!\n");continue;}  //������� 
			newfile(command,false);
		} else if(strcomp(command,"deldir")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//����������Ч��
			if(strlen(command)>=11) {	printf("Name is too long��\n");continue;}
			deldir(command);
		} else if(strcomp(command,"del")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//����������Ч��
			if(strlen(command)>=11) {	printf("Name is too long��\n");continue;}
			deletefile(command,false,0);
		} else if(strcomp(command,"creat")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//����������Ч��
			if(strlen(command)>=11) {	printf("Name is too long��\n");continue;}
			if(checkname(command)){printf("Name is invalid!\n");continue;}	//������� 
			newfile(command,true);
		} else if(strcomp(command,"write")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//����������Ч��
			if(strlen(command)>=11) {	printf("Name is too long��\n");continue;}
			writefile(command);
		} else if(strcomp(command,"read")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//����������Ч��
			if(strlen(command)>=11) {	printf("Name is too long��\n");continue;}
			readfile(command);
		} else if(strcomp(command,"exit")) {
			FILE* fp=fopen("floopy.img","wb");
			fwrite(VirtulMemory, FILESIZE, 1, fp);
			fclose(fp);
			free(VirtulMemory);					//�ͷ��ڴ� 
			break;
		} else printf("Wrong Command��\n");
	}
}

int findfile(const char *ch,u8 Isfile) {
	int loc=locationroot,max;			//Ѱ���ļ����������ļ���λ�� 
	RootEntry* temp_root= (RootEntry*)(VirtulMemory+loc);;
	max=maxrootnum;
	while(!(strcomp(temp_root->DIR_Name,ch)&& temp_root->DIR_Attr==Isfile)&&max>=0) temp_root++,loc+=32,max--;
	if(strcomp(temp_root->DIR_Name,ch)&& temp_root->DIR_Attr==Isfile) return loc;
	else return -1;			//�Ҳ�������-1 
}
bool checkname(const char*ch){
	int loc=locationroot,max;
	RootEntry* temp_root= (RootEntry*)(VirtulMemory+loc);;
	max=maxrootnum;
	while(!(strcomp(temp_root->DIR_Name,ch))&&max>=0) temp_root++,max--;
	if(strcomp(temp_root->DIR_Name,ch)) return true;		//��������򷵻��� 
	else return false;
}
u16 FATread(u16 fatnum,bool Is) {
	int fatpos = fat_base+fatnum*3/2 ;
	bool type;
	if(fatnum%2==0) type=true;						//�ж���ż�� 
	else type=false;
	u16 *bytes_ptr;
	bytes_ptr=(u16 *)(VirtulMemory+fatpos);
	if(type) if(Is) return (*bytes_ptr)%4096;		//��ȡ��1.5���ֽڵ����� 
		else return (*bytes_ptr)/4096;				//��ȡ��0.5���ֽڵ����� 
	else if(Is) return (*bytes_ptr)/16;
	else return (*bytes_ptr)%16;
}

void FATwrite(u16 fatnum,u16 src) {
	int fatpos = fat_base+fatnum*3/2 ;
	bool type;
	if(fatnum%2==0) type=true;
	else type=false;
	u16 ago=FATread(fatnum,false); 		//��ȡ����0.5���ֽڣ����ⱻ���� 
	if(type) {
		src+=ago*4096;		//�������ż����ֿ������� 
	} else {
		src=src*16+ago;
	}
	u16* bytes_ptr=(u16 *)(VirtulMemory+fatpos);
	*bytes_ptr=src;
}
u16 FATfindend(u16 fatnum) {
	u16 bytes = FATread(fatnum,true);	//�ݹ�ֱ���ļ���FAT���ĩβ 
	if(bytes==4095)	return fatnum;
	return FATfindend(bytes);
}
void deleteroot(int loaction) {
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loaction);
	if(strcomp(temp_root->DIR_Name,".")||strcomp(temp_root->DIR_Name,"..")) {
		memset(temp_root,0XF6,sizeof(RootEntry));
		FATwrite(temp_root->DIR_FstClus,0);		//FAT���Ӧ������Ϊ0 
		return;
	} else {
		int max=BLOCKSIZE/32;		//����ļ��� 
		RootEntry* root=(RootEntry*)(VirtulMemory+data_base+(temp_root->DIR_FstClus-2)*BLOCKSIZE);
		while(root->DIR_Attr!=0&&max>=0) {
			if(root->DIR_Attr==0X20) deletefile(temp_root->DIR_Name,true,loaction);	//ɾ���ļ� 
			else if(root->DIR_Attr==0X10) deleteroot(loaction);	//�ݹ�˼�� 
			loaction+=32;
			max--;
			root++;
		}
		memset(temp_root,0XF6,sizeof(RootEntry));			//ɾ����ǰĿ¼ 
	}
}
void cdback(bool Isback,RootEntry* dest) {
	if(dest->DIR_FstClus==0) {		//�����һ���Ǹ�Ŀ¼��ֱ���޸Ĵ�·�� 
		dir=(RootEntry*)(VirtulMemory+root_base);
		strcpy(opendir,"Disk:\\");
		locationroot=root_base;
		maxrootnum=RootEntCnt-1;
	} else {
		locationroot=data_base+(dest->DIR_FstClus-2)*BLOCKSIZE;
		maxrootnum=BLOCKSIZE/32;
		int l=strlen(opendir);
		int loc=l-1;
		if(Isback) {			//����ǻ��ˣ����� strncpy��ȡ��һ��·�� 
			while(opendir[loc]!='\\') loc--;
			char temp[1024];
			memset(temp,0,1024);
			strncpy(temp,opendir,loc);
			strcpy(opendir,temp);
		} else {			//�����ǰ����Ҫ�򿪵�Ŀ¼�����浽·���� 
			if(dir->DIR_Attr!=0X28)strcat(opendir,"\\");
			strcat(opendir,dest->DIR_Name);
		}
		dir=dest;
	}
}
void readchar(int *check,int *num,char **c,RootEntry* temp_root) {
	while((*check)<temp_root->DIR_FileSize&&(*num)<=512) {		//ÿ�ζ�ȡһ���ֽ� 
		printf("%c",**c);
		(*check)++;
		(*num)++;
		(*c)++;
	}
}
int writedata(int location,const char* str,int last,int start) {
	char *c=(char *)(VirtulMemory+data_base+(location-2)*BytsPerSec+start);
	int check=0,l=strlen(str);
	while(check<=BytsPerSec&&l-last>0) {			//ÿ��д��һ���ֽ� 
		*c=str[last];
		last++;
		check++;
		c++;
	}
	return last;			//���ص�ǰд���˵ڼ����ַ��������´μ��� 
}

void newfile(const char*ch,bool IsnewFile) {
	if(ch=='\0') {						//�ж������Ƿ�Ϸ� 
		printf("Wrong command��\n");
		return;
	}
	int loc;
	if(dir->DIR_Attr==0X28) loc=locationroot+32;
	else loc=locationroot;
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	int max=maxrootnum;
	while((temp_root->DIR_Attr==0X10||temp_root->DIR_Attr==0X20)&&(max>=0))temp_root++,max--;
	if(max==-1) {
		printf("There is no empty space��\n");
		return;
	}
	memset(temp_root,0,sizeof(RootEntry));
	strcpy(temp_root->DIR_Name,ch);
	temp_root->DIR_FileSize=0;
	time_t now;
	struct tm *nowtime;
	if(IsnewFile) {				//�ж����½��ļ�����Ŀ¼ 
		temp_root->DIR_Attr=0X20;
		temp_root->DIR_FstClus=0X00;
	} else {
		temp_root->DIR_Attr=0X10;
		u16 temp=2,fats;
		while(FATread(temp,true)!=0&&temp<=maxfats) temp++;		//Ѱ�ҿ���ռ� 
		fats=FATread(temp,true);
		if(fats!=0) {
			printf("There is no enough space��\n");
			return;
		}
		FATwrite(temp,4095);			//д��ĩβ��ֹ�� 
		temp_root->DIR_FstClus=temp;
		RootEntry* temproot=(RootEntry*)(VirtulMemory+data_base+(temp_root->DIR_FstClus-2)*BLOCKSIZE);
		strcpy(temproot->DIR_Name,".");
		temproot->DIR_Attr=0X10;
		temproot->DIR_FstClus=temp;
		now=time(NULL);
		nowtime = localtime(&now);
		temproot->DIR_WrtTime=nowtime->tm_hour * 2048 + nowtime->tm_min * 32 + nowtime->tm_sec / 2;
		temproot->DIR_WrtDate=(nowtime->tm_year - 80) * 512 + (nowtime->tm_mon + 1) * 32 + nowtime->tm_mday;
		temproot++;
		strcpy(temproot->DIR_Name,"..");
		temproot->DIR_Attr=0X10;
		now=time(NULL);
		nowtime = localtime(&now);
		temproot->DIR_WrtTime=nowtime->tm_hour * 2048 + nowtime->tm_min * 32 + nowtime->tm_sec / 2;
		temproot->DIR_WrtDate=(nowtime->tm_year - 80) * 512 + (nowtime->tm_mon + 1) * 32 + nowtime->tm_mday;
		if(dir->DIR_Attr==0X10) temproot->DIR_FstClus=dir->DIR_FstClus;
		else temproot->DIR_FstClus=0X00;			//�����һ��Ŀ¼�Ǵ��̸�Ŀ¼������Ϊ0�����Ժ��ж� 
	}
	now=time(NULL);
	nowtime = localtime(&now);
	temp_root->DIR_WrtTime=nowtime->tm_hour * 2048 + nowtime->tm_min * 32 + nowtime->tm_sec / 2;
	temp_root->DIR_WrtDate=(nowtime->tm_year - 80) * 512 + (nowtime->tm_mon + 1) * 32 + nowtime->tm_mday;
}

void deletefile(const char*ch,bool Isroot,int location) {
	if(ch=='\0') {
		printf("Wrong command��\n");
		return;
	}
	int loc;
	if(Isroot) {        //�ж��Ƿ��ڸ�Ŀ¼�������������� 
		loc=location;
	} else {
		loc=findfile(ch,0X20);
	}
	printf("%d\n",loc);
	if(loc!=-1) {
		RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
		u16 last=temp_root->DIR_FstClus,temp=FATread(last,true);
		while(temp!=4095) {
			last=temp;
			temp=FATread(temp,true);
			FATwrite(last,0);						//FAT���ж�Ӧ�Ĵ�תΪ�մ� 
		}
		FATwrite(temp,0);							//FAT���ж�Ӧ�Ĵ�תΪ�մ� 
		memset(temp_root,0,sizeof(RootEntry));		//������0 
	} else printf("Can't find this file!\n");
}

void showfile() {
	int loc=locationroot;
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	int i=0,t=0;
	while(t<maxrootnum) {  //���ļ�����Ŀ¼���� 
		if(temp_root->DIR_Attr==0X10) {
			printf("Name:%*s<DIR>\t\t\tcreat data:%d/%d/%d %02d:%02d:%02d\n",20,temp_root->DIR_Name, (temp_root->DIR_WrtDate >> 9) + 1980, (temp_root->DIR_WrtDate >> 5) & 0x000f, temp_root->DIR_WrtDate & 0x001f, temp_root->DIR_WrtTime >> 11, (temp_root->DIR_WrtTime >> 5) & 0x003f, temp_root->DIR_WrtTime & 0x001f * 2); //Ŀ¼
			i++;
		}
		t++,temp_root++;
	}
	temp_root=(RootEntry*)(VirtulMemory+loc);
	t=0;
	while(t<maxrootnum) {//���ļ������ļ����� 
		if(temp_root->DIR_Attr==0X20) {
			printf("Name:%*ssize:%d bytes\t\tcreat data:%d/%d/%d %02d:%02d:%02d\n",20,temp_root->DIR_Name,temp_root->DIR_FileSize, (temp_root->DIR_WrtDate >> 9) + 1980, (temp_root->DIR_WrtDate >> 5) & 0x000f, temp_root->DIR_WrtDate & 0x001f, temp_root->DIR_WrtTime >> 11, (temp_root->DIR_WrtTime >> 5) & 0x003f, temp_root->DIR_WrtTime & 0x001f * 2); //�ļ�
			i++;
		}
		t++,temp_root++;
	}
	if(i!=0)printf("�ܹ�%d����¼��\n",i);
	else printf("No file!\n",i);
}

void writefile(const char*ch) {
	if(ch=='\0') {						//���������Ч�� 
		printf("Wrong Command��\n");
		return;
	}
	int loc=findfile(ch,0X20);
	if(loc==-1) {
		printf("Can't find this file��\n");
		return;
	}
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	printf("Name:%s		size:%d	bytes		creat data:%d/%d/%d\t%02d:%02d:%02d\n",temp_root->DIR_Name,temp_root->DIR_FileSize, (temp_root->DIR_WrtDate >> 9) + 1980, (temp_root->DIR_WrtDate >> 5) & 0x000f, temp_root->DIR_WrtDate & 0x001f, temp_root->DIR_WrtTime >> 11, (temp_root->DIR_WrtTime >> 5) & 0x003f, temp_root->DIR_WrtTime & 0x001f * 2); //�ļ�
	if(temp_root->DIR_FileSize!=0)printf("��������ݽ��ӵ��ļ�����ĩβ��\n");
	printf("Please enter��");
	gets(writetemp);
	int l=strlen(writetemp);
	if(l==0) {					//�ж��û�����������Ƿ�Ϊ�� 
		printf("your data is empty��\n");
		return;
	}
	u16 fats=temp_root->DIR_FstClus,lastfats=0;
	u16 temp=2;
	int remain=0;
	if(fats!=0)	{ 			//����ļ���Ϊ�����FAT���ҵ��ļ�ĩβ������д������ 
		lastfats=FATfindend(fats);
		remain=writedata(lastfats,writetemp,0,temp_root->DIR_FileSize%512);
	}
	while(remain<l) {
		while(FATread(temp,true)!=0&&temp<=maxfats) temp++;		//Ѱ��һ����λ 
		fats=FATread(temp,true);
		if(fats==0)	remain=writedata(temp,writetemp,remain,0);	//���Ϊ����д������ 
		else {
			printf("There is no enough space��\n");
			return;
		}
		if(lastfats==0) temp_root->DIR_FstClus=temp;			//�ļ�ָ����״ظı� 
		else FATwrite(lastfats,temp);
		lastfats=temp;
	}
	FATwrite(lastfats,4095);				//��FAT��д������� 
	temp_root->DIR_FileSize+=l;				//�ļ���С�ı� 
}

void readfile(const char*ch) {
	if(ch=='\0'||strcomp("..",ch)||strcomp(".",ch)) {		//���������Ч�� 
		printf("Wrong Command��\n");
		return;
	}
	int loc=findfile(ch,0X20);								//Ѱ���ļ� 
	if(loc==-1) {
		printf("Can't find this file��\n");
		return;
	}
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	printf("Name:%s		size:%d bytes			creat data:%d/%d/%d\t%02d:%02d:%02d\n",temp_root->DIR_Name,temp_root->DIR_FileSize, (temp_root->DIR_WrtDate >> 9) + 1980, (temp_root->DIR_WrtDate >> 5) & 0x000f, temp_root->DIR_WrtDate & 0x001f, temp_root->DIR_WrtTime >> 11, (temp_root->DIR_WrtTime >> 5) & 0x003f, temp_root->DIR_WrtTime & 0x001f * 2); //�ļ�
	if(temp_root->DIR_FileSize==0) {		//�ж��Ƿ�Ϊ�� 
		printf("This file is empty!You can write data to this file.\n");
		return ;
	}
	int check=0,num=0;
	u16 fats=temp_root->DIR_FstClus;
	char* c=(char*)(VirtulMemory+data_base+(fats-2)*BytsPerSec);
	readchar(&check,&num,&c,temp_root);			//�����ֽڵ����ֽڵĶ�ȡ 
	num=0;
	u16 nextfats=FATread(fats,true);			//ת��FAT��ֱ��������Ϊֹ 
	while(nextfats!=4095) {
		c=(char*)(VirtulMemory+data_base+(nextfats-2)*BytsPerSec);
		readchar(&check,&num,&c,temp_root);		//�����ֽڵ����ֽڵĶ�ȡ 
		num=0;
		nextfats=FATread(nextfats,true);
	}
	readchar(&check,&num,&c,temp_root);			//�����ֽڵ����ֽڵĶ�ȡ 
	putchar('\n');								//������һ�����з� 
}
void cd(const char*ch) {
	if(ch=='\0') {
		printf("Wrong Command��\n");			//���������Ч�� 
		return;
	}
	if((strcomp("..",ch)&&dir->DIR_Attr==0X28)||strcomp(".",ch)) {
		return;
	}
	int loc=findfile(ch,0X10);				//����Ŀ¼ 
	if(loc==-1) {
		printf("Can't find this directory��\n");
		return;
	}
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	cdback(strcomp("..",ch),temp_root);				//����Ŀ¼�л����� 
}
void deldir(const char*ch) {
	if(ch=='\0'||strcomp("..",ch)||strcomp(".",ch)) {  //���������Ч�� 
		printf("Wrong Command��\n");
		return;
	}
	int loc=findfile(ch,0X10);						//Ѱ��Ҫɾ����Ŀ¼ 
	int max=BLOCKSIZE/32;
	if(loc==-1) {
		printf("Can't find this directory��\n");
		return;
	}
	RootEntry* temp=(RootEntry*)(VirtulMemory+loc);
	loc=data_base+(temp->DIR_FstClus-2)*BLOCKSIZE;
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	while(temp_root->DIR_Attr==0X20||temp_root->DIR_Attr==0X10&&max>=0) {  ///ѭ���ж�Ŀ¼���ļ����ֿ������� 
		if(temp_root->DIR_Attr==0X20) deletefile(temp_root->DIR_Name,true,loc);
		else if(temp_root->DIR_Attr==0X10) deleteroot(loc);
		loc+=32;
		max--;
	}
	memset(temp,0XF6,sizeof(RootEntry));			//��Ŀ¼��Ϊ0XF6 
}
