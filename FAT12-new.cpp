#include<iostream>
#include<cstdio>
#include<cstring>
#include<cstdlib>
#include<memory.h>
#include<ctime>
using namespace std;
#define BLOCKSIZE 512	//一个扇区大小 
#define FILESIZE 1474560  // 虚拟磁盘空间大小
typedef unsigned char u8;	//1字节
typedef unsigned short u16;	//2字节
typedef unsigned int u32;	//4字节

struct Head {
	char BS_jmpBOOT[3];
	char BS_OEMName[8];
};

struct BPB {
	u16  BPB_BytsPerSec;    //每扇区字节数
	u8 BPB_SecPerClus;      //每簇扇区数
	u16  BPB_RsvdSecCnt;    //Boot记录占用的扇区数
	u8 BPB_NumFATs;	        //FAT表个数
	u16  BPB_RootEntCnt;    //根目录最大文件数
	u16  BPB_TotSec16;      //扇区总数
	u8 BPB_Media;           //介质描述符
	u16  BPB_FATSz16;	   //FAT扇区数
	u16  BPB_SecPerTrk;    //每磁道扇区数
	u16  BPB_NumHeads;     //磁头数
	u32  BPB_HiddSec;      //隐藏扇区数
	u32  BPB_TotSec32;	   //如果BPB_FATSz16为0，该值为FAT扇区数
};

struct BS {
	u8 BS_DrvNum;              //INT 13H的驱动器号
	u8 BS_Reserved1;           //保留，未使用
	u8 BS_BootSig;             //扩展引导标记(29h)
	u32 BS_VolID;			   //卷序列号
	char BS_VolLab[11];          //卷标
	char BS_FileSysType[8];    //文件系统类型
};

//根目录条目
struct RootEntry {
	char DIR_Name[11];
	u8   DIR_Attr;		//文件属性
	char reserved[10];	//保留空间 
	u16  DIR_WrtTime;	//文件写入时间 
	u16  DIR_WrtDate;
	u16  DIR_FstClus;	//开始簇号
	u32  DIR_FileSize;
};

struct fat_c {
	u8 c;
};

u8* VirtulMemory;	//虚拟内存 
int root_base;		//目录区首地址 
int data_base;		//数据区首地址 
int fat_base;		//FAT表首地址 
int maxfats;		//FAT表中簇的最大数目 
BPB *bpb_ptr;
RootEntry* root;
RootEntry* dir;
int RsvdSecCnt;
int NumFATs;
int BytsPerSec;
int RootEntCnt;
BS* bs_ptr;
char openlist[1024];	//存储打开路径 
char *command;			//存储命令后跟的名字 
int locationroot;		//当前打开目录的地址 
int maxrootnum;			//当前打开目录的最大文件数 
char writetemp[4096];	//存储用户写入文件的数据 
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

bool strcomp(const char* a,const char* b); //自定义的字符串比较 
u16 FATread(u16 fatnum,bool Is);			//读取FAT表中的一个簇 
void FATwrite(u16 fatnum,u16 src);			//向FAT表写入一个簇 
u16 FATfindend(u16 fatnum);					//寻找文件在FAT表中的末尾项 
void Init(FILE* f);							//初始化文件 
void menu();								//菜单 
void showfile();							//显示当前目录下的文件 
void newfile(const char *ch,bool IsnewFile);				//新建文件或目录。由 IsnewFile判断 
void deletefile(const char*ch,bool Isroot,int location);	//删除文件， 由 Isroot判断是否在目录下 
int findfile(const char *ch,u8 Isfile);						//寻找文件 
void deleteroot(int loaction);								//删除目录 
void writefile(const char*ch);								//向文件写入数据主函数 
void readfile(const char*ch);								//读取文件 
void InitVal();												//初始化变量 
void cd(const char*ch);										//切换目录 
void deldir(const char*ch);									//删除目录主函数 
void readchar(int *check,int *num,char **c,RootEntry* temp_root);	//一个字节一个字节读取文件数据 
int writedata(int location,const char* str,int last,int start);		//写入数据 
void help();								//帮助信息 
void info();								//磁盘信息 
bool checkname(const char*ch);				//检查文件是否重名 

int main() {
	VirtulMemory = (u8 *)malloc(FILESIZE);  //申请虚拟磁盘空间
	FILE* fp=fopen("floopy.img","rb");
	if(fp==NULL) Init(fp);					//文件不存在则创建一个 
	else {
		fread(VirtulMemory,FILESIZE,1,fp);
		fclose(fp);
		bpb_ptr=(BPB *)(VirtulMemory+11);
		bs_ptr=(BS*)(VirtulMemory+36);
	}
	InitVal();								//初始化变量 
	locationroot=root_base;
	maxrootnum=RootEntCnt-1;
	menu();
	return 0;
}

//自定义字符串比较
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
//	printf("请输入磁盘名:");
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
	for(i=1; i<=FILESIZE-16896; i++,charc++) charc->c=0XF6;   		//将数据区的字节全部转为0XF6 
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
	printf("Please enter a disk name：");
	char name[11];
	scanf("%s",name);
	getchar();
	creatFile(name);
	system("cls");
}
void info() {
	printf("卷标:%s\n",bs_ptr->BS_VolLab);
	printf("文件系统类型:%s\n",bs_ptr->BS_FileSysType);
	printf("磁盘大小:%d字节\n",FILESIZE);
	printf("磁盘OEM:LveYYn\n");
	printf("每扇区字节数:%d\n",BytsPerSec);
	printf("每簇扇区数:%d\n",bpb_ptr->BPB_SecPerClus);
	printf("FAT表个数:2\n");
	printf("Boot记录占用的扇区数:%d\n",RsvdSecCnt);
	printf("根目录最大文件数:%d\n",RootEntCnt);
	printf("FAT扇区数:%d\n",bpb_ptr->BPB_FATSz16);
	printf("FAT表起始位置:%d字节\n",fat_base);
	printf("根目录起始位置:%d字节\n",root_base);
	printf("数据区起始位置:%d字节\n",data_base);
}
void help() {
	printf("*********************************************************************\n");
	printf("usage:\n");
	printf("	format <newname>:   			格式化磁盘\n");
	printf("	ls:   					浏览所有文件\n");
	printf("	cd <dirname>:   			切换目录(..回退)\n");
	printf("	mkdir <dirname>:   			新建目录\n");
	printf("	deldir <dirname>:   			删除目录\n");
	printf("	creat <filename>:   			新建文件\n");
	printf("	del <filename>:   			删除文件\n");
	printf("	write <filename>:   			写入文件\n");
	printf("	read <filename>:   			读取文件\n");
	printf("	cls:   					清屏\n");
	printf("	info:   				显示磁盘信息\n");
	printf("	help:   				操作命令详情\n");
	printf("	exit:   				保存并退出\n");
	printf("*********************************************************************\n");
}
void menu() {
	printf("Welcome to my fat12 simulator system beta edition.\n");
	printf("You can enter 'help' for more information.\n");
	while(1) {
		printf("%s>",opendir);
		gets(openlist);
		if(strlen(openlist)==0) continue;				//检查命令的有效性 
		command=strtok(openlist," ");					//将命令分解 
		if(command==NULL) continue;						//检查命令的有效性
		if(strcomp(command,"format")) {
			command=strtok(NULL," ");					//将命令分解 
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//检查命令的有效性
			if(command=='\0'||strlen(command)>=11) {
				printf("Wrong Command！\n");
				continue;
			}
			if(strlen(command)>=11) {	printf("Name is too long！\n");continue;}
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
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//检查命令的有效性
			if(strlen(command)>=11) {	printf("Name is too long！\n");continue;}
			cd(command);
		} else if(strcomp(command,"mkdir")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//检查命令的有效性
			if(strlen(command)>=11) {	printf("Name is too long！\n");continue;}
			if(checkname(command)){printf("Name is invalid!\n");continue;}  //检查重名 
			newfile(command,false);
		} else if(strcomp(command,"deldir")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//检查命令的有效性
			if(strlen(command)>=11) {	printf("Name is too long！\n");continue;}
			deldir(command);
		} else if(strcomp(command,"del")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//检查命令的有效性
			if(strlen(command)>=11) {	printf("Name is too long！\n");continue;}
			deletefile(command,false,0);
		} else if(strcomp(command,"creat")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//检查命令的有效性
			if(strlen(command)>=11) {	printf("Name is too long！\n");continue;}
			if(checkname(command)){printf("Name is invalid!\n");continue;}	//检查重名 
			newfile(command,true);
		} else if(strcomp(command,"write")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//检查命令的有效性
			if(strlen(command)>=11) {	printf("Name is too long！\n");continue;}
			writefile(command);
		} else if(strcomp(command,"read")) {
			command=strtok(NULL," ");
			if(command==NULL) {printf("Name is invalid!\n");continue;}		//检查命令的有效性
			if(strlen(command)>=11) {	printf("Name is too long！\n");continue;}
			readfile(command);
		} else if(strcomp(command,"exit")) {
			FILE* fp=fopen("floopy.img","wb");
			fwrite(VirtulMemory, FILESIZE, 1, fp);
			fclose(fp);
			free(VirtulMemory);					//释放内存 
			break;
		} else printf("Wrong Command！\n");
	}
}

int findfile(const char *ch,u8 Isfile) {
	int loc=locationroot,max;			//寻找文件，并返回文件的位置 
	RootEntry* temp_root= (RootEntry*)(VirtulMemory+loc);;
	max=maxrootnum;
	while(!(strcomp(temp_root->DIR_Name,ch)&& temp_root->DIR_Attr==Isfile)&&max>=0) temp_root++,loc+=32,max--;
	if(strcomp(temp_root->DIR_Name,ch)&& temp_root->DIR_Attr==Isfile) return loc;
	else return -1;			//找不到返回-1 
}
bool checkname(const char*ch){
	int loc=locationroot,max;
	RootEntry* temp_root= (RootEntry*)(VirtulMemory+loc);;
	max=maxrootnum;
	while(!(strcomp(temp_root->DIR_Name,ch))&&max>=0) temp_root++,max--;
	if(strcomp(temp_root->DIR_Name,ch)) return true;		//如果重名则返回真 
	else return false;
}
u16 FATread(u16 fatnum,bool Is) {
	int fatpos = fat_base+fatnum*3/2 ;
	bool type;
	if(fatnum%2==0) type=true;						//判断奇偶项 
	else type=false;
	u16 *bytes_ptr;
	bytes_ptr=(u16 *)(VirtulMemory+fatpos);
	if(type) if(Is) return (*bytes_ptr)%4096;		//读取出1.5个字节的数据 
		else return (*bytes_ptr)/4096;				//读取出0.5个字节的数据 
	else if(Is) return (*bytes_ptr)/16;
	else return (*bytes_ptr)%16;
}

void FATwrite(u16 fatnum,u16 src) {
	int fatpos = fat_base+fatnum*3/2 ;
	bool type;
	if(fatnum%2==0) type=true;
	else type=false;
	u16 ago=FATread(fatnum,false); 		//读取另外0.5个字节，避免被覆盖 
	if(type) {
		src+=ago*4096;		//奇数项和偶数项分开来处理 
	} else {
		src=src*16+ago;
	}
	u16* bytes_ptr=(u16 *)(VirtulMemory+fatpos);
	*bytes_ptr=src;
}
u16 FATfindend(u16 fatnum) {
	u16 bytes = FATread(fatnum,true);	//递归直到文件在FAT表的末尾 
	if(bytes==4095)	return fatnum;
	return FATfindend(bytes);
}
void deleteroot(int loaction) {
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loaction);
	if(strcomp(temp_root->DIR_Name,".")||strcomp(temp_root->DIR_Name,"..")) {
		memset(temp_root,0XF6,sizeof(RootEntry));
		FATwrite(temp_root->DIR_FstClus,0);		//FAT表对应簇重置为0 
		return;
	} else {
		int max=BLOCKSIZE/32;		//最多文件数 
		RootEntry* root=(RootEntry*)(VirtulMemory+data_base+(temp_root->DIR_FstClus-2)*BLOCKSIZE);
		while(root->DIR_Attr!=0&&max>=0) {
			if(root->DIR_Attr==0X20) deletefile(temp_root->DIR_Name,true,loaction);	//删除文件 
			else if(root->DIR_Attr==0X10) deleteroot(loaction);	//递归思想 
			loaction+=32;
			max--;
			root++;
		}
		memset(temp_root,0XF6,sizeof(RootEntry));			//删除当前目录 
	}
}
void cdback(bool Isback,RootEntry* dest) {
	if(dest->DIR_FstClus==0) {		//如果上一级是根目录则直接修改打开路径 
		dir=(RootEntry*)(VirtulMemory+root_base);
		strcpy(opendir,"Disk:\\");
		locationroot=root_base;
		maxrootnum=RootEntCnt-1;
	} else {
		locationroot=data_base+(dest->DIR_FstClus-2)*BLOCKSIZE;
		maxrootnum=BLOCKSIZE/32;
		int l=strlen(opendir);
		int loc=l-1;
		if(Isback) {			//如果是回退，则用 strncpy截取上一级路径 
			while(opendir[loc]!='\\') loc--;
			char temp[1024];
			memset(temp,0,1024);
			strncpy(temp,opendir,loc);
			strcpy(opendir,temp);
		} else {			//如果是前进则将要打开的目录名保存到路径里 
			if(dir->DIR_Attr!=0X28)strcat(opendir,"\\");
			strcat(opendir,dest->DIR_Name);
		}
		dir=dest;
	}
}
void readchar(int *check,int *num,char **c,RootEntry* temp_root) {
	while((*check)<temp_root->DIR_FileSize&&(*num)<=512) {		//每次读取一个字节 
		printf("%c",**c);
		(*check)++;
		(*num)++;
		(*c)++;
	}
}
int writedata(int location,const char* str,int last,int start) {
	char *c=(char *)(VirtulMemory+data_base+(location-2)*BytsPerSec+start);
	int check=0,l=strlen(str);
	while(check<=BytsPerSec&&l-last>0) {			//每次写入一个字节 
		*c=str[last];
		last++;
		check++;
		c++;
	}
	return last;			//返回当前写到了第几个字符，方便下次继续 
}

void newfile(const char*ch,bool IsnewFile) {
	if(ch=='\0') {						//判断命令是否合法 
		printf("Wrong command！\n");
		return;
	}
	int loc;
	if(dir->DIR_Attr==0X28) loc=locationroot+32;
	else loc=locationroot;
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	int max=maxrootnum;
	while((temp_root->DIR_Attr==0X10||temp_root->DIR_Attr==0X20)&&(max>=0))temp_root++,max--;
	if(max==-1) {
		printf("There is no empty space！\n");
		return;
	}
	memset(temp_root,0,sizeof(RootEntry));
	strcpy(temp_root->DIR_Name,ch);
	temp_root->DIR_FileSize=0;
	time_t now;
	struct tm *nowtime;
	if(IsnewFile) {				//判断是新建文件还是目录 
		temp_root->DIR_Attr=0X20;
		temp_root->DIR_FstClus=0X00;
	} else {
		temp_root->DIR_Attr=0X10;
		u16 temp=2,fats;
		while(FATread(temp,true)!=0&&temp<=maxfats) temp++;		//寻找空余空间 
		fats=FATread(temp,true);
		if(fats!=0) {
			printf("There is no enough space！\n");
			return;
		}
		FATwrite(temp,4095);			//写入末尾截止符 
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
		else temproot->DIR_FstClus=0X00;			//如果上一级目录是磁盘根目录，则设为0方便以后判断 
	}
	now=time(NULL);
	nowtime = localtime(&now);
	temp_root->DIR_WrtTime=nowtime->tm_hour * 2048 + nowtime->tm_min * 32 + nowtime->tm_sec / 2;
	temp_root->DIR_WrtDate=(nowtime->tm_year - 80) * 512 + (nowtime->tm_mon + 1) * 32 + nowtime->tm_mday;
}

void deletefile(const char*ch,bool Isroot,int location) {
	if(ch=='\0') {
		printf("Wrong command！\n");
		return;
	}
	int loc;
	if(Isroot) {        //判断是否在根目录区还是在数据区 
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
			FATwrite(last,0);						//FAT表中对应的簇转为空簇 
		}
		FATwrite(temp,0);							//FAT表中对应的簇转为空簇 
		memset(temp_root,0,sizeof(RootEntry));		//数据清0 
	} else printf("Can't find this file!\n");
}

void showfile() {
	int loc=locationroot;
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	int i=0,t=0;
	while(t<maxrootnum) {  //对文件排序，目录在上 
		if(temp_root->DIR_Attr==0X10) {
			printf("Name:%*s<DIR>\t\t\tcreat data:%d/%d/%d %02d:%02d:%02d\n",20,temp_root->DIR_Name, (temp_root->DIR_WrtDate >> 9) + 1980, (temp_root->DIR_WrtDate >> 5) & 0x000f, temp_root->DIR_WrtDate & 0x001f, temp_root->DIR_WrtTime >> 11, (temp_root->DIR_WrtTime >> 5) & 0x003f, temp_root->DIR_WrtTime & 0x001f * 2); //目录
			i++;
		}
		t++,temp_root++;
	}
	temp_root=(RootEntry*)(VirtulMemory+loc);
	t=0;
	while(t<maxrootnum) {//对文件排序，文件在下 
		if(temp_root->DIR_Attr==0X20) {
			printf("Name:%*ssize:%d bytes\t\tcreat data:%d/%d/%d %02d:%02d:%02d\n",20,temp_root->DIR_Name,temp_root->DIR_FileSize, (temp_root->DIR_WrtDate >> 9) + 1980, (temp_root->DIR_WrtDate >> 5) & 0x000f, temp_root->DIR_WrtDate & 0x001f, temp_root->DIR_WrtTime >> 11, (temp_root->DIR_WrtTime >> 5) & 0x003f, temp_root->DIR_WrtTime & 0x001f * 2); //文件
			i++;
		}
		t++,temp_root++;
	}
	if(i!=0)printf("总共%d个记录项\n",i);
	else printf("No file!\n",i);
}

void writefile(const char*ch) {
	if(ch=='\0') {						//检查命令有效性 
		printf("Wrong Command！\n");
		return;
	}
	int loc=findfile(ch,0X20);
	if(loc==-1) {
		printf("Can't find this file！\n");
		return;
	}
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	printf("Name:%s		size:%d	bytes		creat data:%d/%d/%d\t%02d:%02d:%02d\n",temp_root->DIR_Name,temp_root->DIR_FileSize, (temp_root->DIR_WrtDate >> 9) + 1980, (temp_root->DIR_WrtDate >> 5) & 0x000f, temp_root->DIR_WrtDate & 0x001f, temp_root->DIR_WrtTime >> 11, (temp_root->DIR_WrtTime >> 5) & 0x003f, temp_root->DIR_WrtTime & 0x001f * 2); //文件
	if(temp_root->DIR_FileSize!=0)printf("输入的数据将接到文件已有末尾！\n");
	printf("Please enter：");
	gets(writetemp);
	int l=strlen(writetemp);
	if(l==0) {					//判断用户输入的数据是否为空 
		printf("your data is empty！\n");
		return;
	}
	u16 fats=temp_root->DIR_FstClus,lastfats=0;
	u16 temp=2;
	int remain=0;
	if(fats!=0)	{ 			//如果文件不为空则从FAT表找到文件末尾，继续写入数据 
		lastfats=FATfindend(fats);
		remain=writedata(lastfats,writetemp,0,temp_root->DIR_FileSize%512);
	}
	while(remain<l) {
		while(FATread(temp,true)!=0&&temp<=maxfats) temp++;		//寻找一个空位 
		fats=FATread(temp,true);
		if(fats==0)	remain=writedata(temp,writetemp,remain,0);	//如果为空则写入数据 
		else {
			printf("There is no enough space！\n");
			return;
		}
		if(lastfats==0) temp_root->DIR_FstClus=temp;			//文件指向的首簇改变 
		else FATwrite(lastfats,temp);
		lastfats=temp;
	}
	FATwrite(lastfats,4095);				//在FAT表写入结束符 
	temp_root->DIR_FileSize+=l;				//文件大小改变 
}

void readfile(const char*ch) {
	if(ch=='\0'||strcomp("..",ch)||strcomp(".",ch)) {		//检查命令有效性 
		printf("Wrong Command！\n");
		return;
	}
	int loc=findfile(ch,0X20);								//寻找文件 
	if(loc==-1) {
		printf("Can't find this file！\n");
		return;
	}
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	printf("Name:%s		size:%d bytes			creat data:%d/%d/%d\t%02d:%02d:%02d\n",temp_root->DIR_Name,temp_root->DIR_FileSize, (temp_root->DIR_WrtDate >> 9) + 1980, (temp_root->DIR_WrtDate >> 5) & 0x000f, temp_root->DIR_WrtDate & 0x001f, temp_root->DIR_WrtTime >> 11, (temp_root->DIR_WrtTime >> 5) & 0x003f, temp_root->DIR_WrtTime & 0x001f * 2); //文件
	if(temp_root->DIR_FileSize==0) {		//判断是否为空 
		printf("This file is empty!You can write data to this file.\n");
		return ;
	}
	int check=0,num=0;
	u16 fats=temp_root->DIR_FstClus;
	char* c=(char*)(VirtulMemory+data_base+(fats-2)*BytsPerSec);
	readchar(&check,&num,&c,temp_root);			//单个字节单个字节的读取 
	num=0;
	u16 nextfats=FATread(fats,true);			//转向FAT表，直到结束符为止 
	while(nextfats!=4095) {
		c=(char*)(VirtulMemory+data_base+(nextfats-2)*BytsPerSec);
		readchar(&check,&num,&c,temp_root);		//单个字节单个字节的读取 
		num=0;
		nextfats=FATread(nextfats,true);
	}
	readchar(&check,&num,&c,temp_root);			//单个字节单个字节的读取 
	putchar('\n');								//最后加上一个换行符 
}
void cd(const char*ch) {
	if(ch=='\0') {
		printf("Wrong Command！\n");			//检查命令有效性 
		return;
	}
	if((strcomp("..",ch)&&dir->DIR_Attr==0X28)||strcomp(".",ch)) {
		return;
	}
	int loc=findfile(ch,0X10);				//查找目录 
	if(loc==-1) {
		printf("Can't find this directory！\n");
		return;
	}
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	cdback(strcomp("..",ch),temp_root);				//进行目录切换动作 
}
void deldir(const char*ch) {
	if(ch=='\0'||strcomp("..",ch)||strcomp(".",ch)) {  //检查命令有效性 
		printf("Wrong Command！\n");
		return;
	}
	int loc=findfile(ch,0X10);						//寻找要删除的目录 
	int max=BLOCKSIZE/32;
	if(loc==-1) {
		printf("Can't find this directory！\n");
		return;
	}
	RootEntry* temp=(RootEntry*)(VirtulMemory+loc);
	loc=data_base+(temp->DIR_FstClus-2)*BLOCKSIZE;
	RootEntry* temp_root=(RootEntry*)(VirtulMemory+loc);
	while(temp_root->DIR_Attr==0X20||temp_root->DIR_Attr==0X10&&max>=0) {  ///循环判断目录或文件，分开来处理 
		if(temp_root->DIR_Attr==0X20) deletefile(temp_root->DIR_Name,true,loc);
		else if(temp_root->DIR_Attr==0X10) deleteroot(loc);
		loc+=32;
		max--;
	}
	memset(temp,0XF6,sizeof(RootEntry));			//将目录补为0XF6 
}
