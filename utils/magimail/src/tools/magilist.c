#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

#include <shared/types.h>
#include <shared/jbstrcpy.h>
#include <shared/parseargs.h>
#include <shared/jblist.h>
#include <shared/path.h>
#include <shared/mystrncpy.h>

#include <oslib/os.h>
#include <oslib/osmem.h>
#include <oslib/osfile.h>
#include <oslib/osdir.h>
#include <oslib/osmisc.h>

#include <magimail/version.h>

#ifdef PLATFORM_AMIGA
char *ver="$VER: CrashList " VERSION " " __AMIGADATE__;
#endif

#define ARG_DIRECTORY 0

struct argument args[] =
   { { ARGTYPE_STRING, "DIRECTORY", ARGFLAG_AUTO,  NULL },
     { ARGTYPE_END,    NULL,        0,             0    } };

struct idx
{
	uint16_t zone,net,node,point,region,hub;
	uint32_t offset;
};

bool nomem,diskfull;

void putuword(char *buf,uint32_t offset,uint16_t num)
{
   buf[offset]=num%256;
   buf[offset+1]=num/256;
}

void putulong(char *buf,uint32_t offset,uint32_t num)
{
   buf[offset]=num%256;
   buf[offset+1]=(num / 256) % 256;
   buf[offset+2]=(num / 256 / 256) % 256;
   buf[offset+3]=(num / 256 / 256 / 256) % 256;
}

void WriteIdx(osFile fh,struct idx *idx)
{
	char binbuf[16];

	putuword(binbuf,0,idx->zone);
	putuword(binbuf,2,idx->net);
	putuword(binbuf,4,idx->node);
	putuword(binbuf,6,idx->point);
	putuword(binbuf,8,idx->region);
	putuword(binbuf,10,idx->hub);
	putulong(binbuf,12,idx->offset);

	osWrite(fh,binbuf,sizeof(binbuf));
}

char nlname[100];
char *findfile,*finddir;
time_t newest;

bool isnodelistending(char *name)
{
   if(strlen(name)<4)   return(FALSE);

   if(name[strlen(name)-4]!='.') return(FALSE);

   if(!isdigit(name[strlen(name)-3]))   return(FALSE);
   if(!isdigit(name[strlen(name)-2]))   return(FALSE);
   if(!isdigit(name[strlen(name)-1]))   return(FALSE);

   return(TRUE);
}
                                                 
void scandirfunc(char *file)
{
	char buf[500];
	struct osFileEntry *fe;

	if(isnodelistending(file))
   {
   	if(strnicmp(file,findfile,strlen(file)-4)==0)
      {
			MakeFullPath(finddir,file,buf,500);
			
			if((fe=osGetFileEntry(buf)))
			{
				if(nlname[0]==0 || newest < fe->Date)
				{
					mystrncpy(nlname,fe->Name,100);
					newest=fe->Date;
				}
				
				osFree(fe);
			}
		}
	}
}

bool FindList(char *dir,char *file,char *dest)
{
	MakeFullPath(dir,file,dest,500);
	
	if(osExists(dest))
		return(TRUE);

	nlname[0]=0;
	newest=0;
	findfile=file;
	finddir=dir;
			
   if(!osScanDir(dir,scandirfunc))
   {
		uint32_t err=osError();
      printf("Failed to scan directory %s\n",dir);
		printf("Error: %s\n",osErrorMsg(err));		
      return(FALSE);
   }

	if(nlname[0]==0)
	{
		printf("Found no nodelist matching %s in %s\n",file,dir);
		return(FALSE);
	}
	
	MakeFullPath(dir,nlname,dest,500);

	return(TRUE);
}

void ProcessList(char *dir,char *file,osFile ifh,uint16_t defzone)
{
	struct idx idx;
	char buf[500];
	osFile nfh;
	
	if(!FindList(dir,file,buf))
		return;
	
	if(!(nfh=osOpen(buf,MODE_OLDFILE)))
	{
		uint32_t err=osError();
		printf("Failed to read %s\n",buf);
		printf("Error: %s\n",osErrorMsg(err));		
		return;
	}

	strcpy(buf,(char *)GetFilePart(buf));
	printf("Processing nodelist %s...\n",buf);
	osWrite(ifh,buf,100);

	idx.zone=defzone;
	idx.net=0;
	idx.node=0;
	idx.point=0;
	idx.region=0;
	idx.hub=0;
	idx.offset=0;
	
	idx.offset=osFTell(nfh);	

   while(osFGets(nfh,buf,500))
   {
		if(strnicmp(buf,"Zone,",5)==0)
		{
			idx.zone=atoi(&buf[5]);
			idx.region=0;
			idx.net=idx.zone;
			idx.hub=0;
			idx.node=0;
			idx.point=0;
			WriteIdx(ifh,&idx);
		}
		
		if(strnicmp(buf,"Region,",7)==0)
		{
			idx.region=atoi(&buf[7]);
			idx.net=idx.region;
			idx.hub=0;
			idx.node=0;
			idx.point=0;
			WriteIdx(ifh,&idx);
		}

		if(strnicmp(buf,"Host,",5)==0)
		{
			idx.net=atoi(&buf[5]);
			idx.hub=0;
			idx.node=0;
			idx.point=0;
			WriteIdx(ifh,&idx);
		}

		if(strnicmp(buf,"Hub,",4)==0)
		{
			idx.hub=atoi(&buf[4]);
			idx.node=idx.hub;
			idx.point=0;
			WriteIdx(ifh,&idx);
		}

		if(strnicmp(buf,"Pvt,",4)==0)
		{
			idx.node=atoi(&buf[4]);
			idx.point=0;
			WriteIdx(ifh,&idx);
		}

		if(strnicmp(buf,"Hold,",5)==0)
		{
			idx.node=atoi(&buf[5]);
			idx.point=0;
			WriteIdx(ifh,&idx);
		}

		if(strnicmp(buf,",",1)==0)
		{
			idx.node=atoi(&buf[1]);
			idx.point=0;
			WriteIdx(ifh,&idx);
		}

		if(strnicmp(buf,"Point,",6)==0)
		{
			idx.point=atoi(&buf[6]);
			WriteIdx(ifh,&idx);
		}

		idx.offset=osFTell(nfh);	
	}

	idx.zone=0;
	idx.region=0;
	idx.net=0;
	idx.hub=0;
	idx.node=0;
	idx.point=0;
	idx.offset=0xffffffff;

	WriteIdx(ifh,&idx);
}

int main(int argc, char **argv)
{
   osFile lfh,ifh;
   char *dir,buf[200],cfgbuf[200],file[100];
   uint32_t jbcpos,zone;  

   if(!osInit())
      exit(OS_EXIT_ERROR);

   if(argc > 1 &&
	  (strcmp(argv[1],"?")==0      ||
		strcmp(argv[1],"-h")==0     ||
		strcmp(argv[1],"--help")==0 ||
		strcmp(argv[1],"help")==0 ||
		strcmp(argv[1],"/h")==0     ||
		strcmp(argv[1],"/?")==0 ))
   {
      printargs(args);
      osEnd();
      exit(OS_EXIT_OK);
   }

   if(!parseargs(args,argc,argv))
   {
      osEnd();
      exit(OS_EXIT_ERROR);
   }
   
	dir=OS_CURRENT_DIR;
	
   if(args[ARG_DIRECTORY].data)
		dir=(char *)args[ARG_DIRECTORY].data;

	MakeFullPath(dir,"cmnodelist.prefs",buf,200);
		
   if(!(lfh=osOpen(buf,MODE_OLDFILE)))
   {
		uint32_t err=osError();
      printf("Failed to open %s for reading\n",buf);
		printf("Error: %s\n",osErrorMsg(err));		
      osEnd();
      exit(OS_EXIT_ERROR);
   }

	MakeFullPath(dir,"cmnodelist.index",buf,200);

   if(!(ifh=osOpen(buf,MODE_NEWFILE)))
   {
		uint32_t err=osError();
      printf("Failed to open %s for writing (nodelist in use?)\n",buf);
		printf("Error: %s\n",osErrorMsg(err));		
		osClose(lfh);
      osEnd();
      exit(OS_EXIT_ERROR);
   }

 	osWrite(ifh,"CNL1",4);
 
   while(osFGets(lfh,cfgbuf,200))
   {
		if(cfgbuf[0]!=';')
		{		
	      jbcpos=0;

	      if(jbstrcpy(file,cfgbuf,100,&jbcpos))
			{
				zone=0;
			
				if(jbstrcpy(buf,cfgbuf,10,&jbcpos))
					zone=atoi(buf);
				
				ProcessList(dir,file,ifh,zone);
			}
	   }
	}
	
	osClose(lfh);
	osClose(ifh);

   osEnd();
   
   exit(OS_EXIT_OK);
}


/* Hitta r�tt nodelista */

