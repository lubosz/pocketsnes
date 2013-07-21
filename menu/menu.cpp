
#include "sal.h"
#include "menu.h"
#include "snapshot.h"
#include "snes9x.h"
#include "gfx.h"
#include "memmap.h"
#include "soundux.h"
#include "savestateio.h"

#define MAX_DISPLAY_CHARS			40

#define ROM_SELECTOR_SAVE_DEFAULT_DIR		0
#define ROM_SELECTOR_MAIN_MENU			1
#define ROM_SELECTOR_DEFAULT_FOCUS		2
#define ROM_SELECTOR_ROM_START			3

static u16 mMenuTile[64*64];
static u16 mMenuHeader[320*48];
static u16 mHighLightBar[320*16];

static s32 mMenutileXscroll=0;
static s32 mMenutileYscroll=0;
static s32 mTileCounter=0;
static s32 mQuickSavePresent=0;

static s8 mMenuText[30][MAX_DISPLAY_CHARS];

static struct SAL_DIRECTORY_ENTRY *mRomList=NULL;
static s32 mRomCount;
static s8 mRomDir[SAL_MAX_PATH]={""};

static struct SAVE_STATE mSaveState[10];  // holds the filenames for the savestate and "inuse" flags
static s8 mSaveStateName[SAL_MAX_PATH]={""};       // holds the last filename to be scanned for save states
static s8 mRomName[SAL_MAX_PATH]={""};
static s8 mSystemDir[SAL_MAX_PATH];
static struct MENU_OPTIONS *mMenuOptions=NULL;
static u16 mTempFb[SAL_SCREEN_WIDTH*SAL_SCREEN_HEIGHT];

									
void DefaultMenuOptions(void)
{
	mMenuOptions->frameSkip=0;   //auto
	mMenuOptions->soundEnabled = 1; 
	mMenuOptions->volume=25; 
	mMenuOptions->cpuSpeed=336; 
	mMenuOptions->country=0;
	mMenuOptions->showFps=1;
	mMenuOptions->soundRate=44100;
	mMenuOptions->stereo=1;
	mMenuOptions->fullScreen=0;
	mMenuOptions->autoSaveSram=0;
}

s32 LoadMenuOptions(const char *path, const char *filename, const char *ext,
			const char *optionsmem, s32 maxSize, s32 showMessage)
{
	s8 fullFilename[SAL_MAX_PATH];
	s8 _filename[SAL_MAX_PATH];
	s8 _ext[SAL_MAX_PATH];
	s8 _path[SAL_MAX_PATH];
	s32 size=0;

	sal_DirectorySplitFilename(filename, _path, _filename, _ext);
	sprintf(fullFilename,"%s%s%s.%s",path,SAL_DIR_SEP,_filename,ext);
	return sal_FileLoad(fullFilename,(u8*)optionsmem,maxSize,(u32*)&size);
}

s32 SaveMenuOptions(const char *path, const char *filename, const char *ext,
			const char *optionsmem, s32 maxSize, s32 showMessage)
{
	s8 fullFilename[SAL_MAX_PATH];
	s8 _filename[SAL_MAX_PATH];
	s8 _ext[SAL_MAX_PATH];
	s8 _path[SAL_MAX_PATH];
	
	if (showMessage)
	{
		PrintTile();
		PrintTitle("");
		sal_VideoPrint(8,120,"Saving...",SAL_RGB(31,31,31));
		sal_VideoFlip(1);
	}

	sal_DirectorySplitFilename(filename, _path, _filename, _ext);
	sprintf(fullFilename,"%s%s%s.%s",path,SAL_DIR_SEP,_filename,ext);
	return sal_FileSave(fullFilename,(u8*)optionsmem,maxSize);
}

s32 DeleteMenuOptions(const char *path, const char *filename,
			const char *ext, s32 showMessage)
{
	s8 fullFilename[SAL_MAX_PATH];
	s8 _filename[SAL_MAX_PATH];
	s8 _ext[SAL_MAX_PATH];
	s8 _path[SAL_MAX_PATH];
	
	if (showMessage)
	{
		PrintTile();
		PrintTitle("");
		sal_VideoPrint(8,120,"Deleting...",SAL_RGB(31,31,31));
		sal_VideoFlip(1);
	}

	sal_DirectorySplitFilename(filename, _path, _filename, _ext);
	sprintf(fullFilename,"%s%s%s.%s",path,SAL_DIR_SEP,_filename,ext);
	sal_FileDelete(fullFilename);
	return SAL_OK;
}

void MenuPause()
{
	sal_InputWaitForPress();
	sal_InputWaitForRelease();
}

s32 MenuMessageBox(const char *message1, const char *message2,
			const char *message3, enum MENU_MESSAGE_BOX_MODE mode)
{
  s32 select=0;
  s32 subaction=-1;
  u32 keys=0;
  
  sal_InputIgnore();
  while(subaction==-1)
  {
     keys=sal_InputPollRepeat();
     if (keys & SAL_INPUT_UP) 
     {
       select=SAL_OK; // Up
     }
     if (keys & SAL_INPUT_DOWN) 
     {
       select=SAL_ERROR; // Down
     }
     if ((keys&INP_BUTTON_MENU_SELECT) || (keys&INP_BUTTON_MENU_CANCEL))
     {
        subaction=select;
     }
     PrintTile();
     PrintTitle("Message Box");
     sal_VideoPrint(8,50,message1,SAL_RGB(31,31,31));
     sal_VideoPrint(8,60,message2,SAL_RGB(31,31,31));
     sal_VideoPrint(8,70,message3,SAL_RGB(31,31,31));
     switch(mode)
     {
        case MENU_MESSAGE_BOX_MODE_YESNO: // yes no input
	       if(select==SAL_OK)
	       {
			  PrintBar(120-4);
	          sal_VideoPrint(8,120,"YES",SAL_RGB(0,0,0));
	          sal_VideoPrint(8,140,"NO",SAL_RGB(31,31,31));
	       }
	       else
	       {
			  PrintBar(140-4);
	          sal_VideoPrint(8,120,"YES",SAL_RGB(31,31,31));
	          sal_VideoPrint(8,140,"NO",SAL_RGB(0,0,0));
	       }
	       break;
	case MENU_MESSAGE_BOX_MODE_PAUSE: 
			PrintBar(120-4);
			sal_VideoPrint(8,120,"Press button to continue",SAL_RGB(0,0,0));
			break;
	case MENU_MESSAGE_BOX_MODE_MSG:
			subaction=SAL_OK;
			break;
     }
     sal_VideoFlip(1);
  }
  sal_InputIgnore();
  return(subaction);
}

void PrintTile()
{
	sal_ImageDrawTiled(mMenuTile, MENU_TILE_WIDTH, MENU_TILE_HEIGHT, mMenutileXscroll, mMenutileYscroll, 0, 48);	

	mTileCounter++;
	if (mTileCounter > 1)
	{
		mTileCounter=0;
		mMenutileXscroll++;
		if(mMenutileXscroll>=MENU_TILE_WIDTH) mMenutileXscroll=0;
	  
		mMenutileYscroll++;
		if(mMenutileYscroll>=MENU_TILE_HEIGHT) mMenutileYscroll=0;
	}  
	return; 
}

void PrintTitle(const char *title)
{	
	sal_ImageDraw(mMenuHeader,MENU_HEADER_WIDTH, MENU_HEADER_HEIGHT,0,0);	
}

void PrintBar(u32 givenY)
{
	sal_ImageDraw(mHighLightBar,HIGHLIGHT_BAR_WIDTH, HIGHLIGHT_BAR_HEIGHT,0,givenY);
}

void freeRomLists()
{
	//free rom list buffers
	if(mRomList != NULL) free(mRomList);
	mRomList = NULL;
}

void DefaultRomListItems()
{
	s32 i;

	strcpy(mRomList[ROM_SELECTOR_SAVE_DEFAULT_DIR].displayName,"Save Default Directory");
	strcpy(mRomList[ROM_SELECTOR_MAIN_MENU].displayName,"Main Menu");
	mRomList[ROM_SELECTOR_DEFAULT_FOCUS].displayName[0]=0;
}

static
void SwapDirectoryEntry(struct SAL_DIRECTORY_ENTRY *salFrom, struct SAL_DIRECTORY_ENTRY *salTo)
{
	struct SAL_DIRECTORY_ENTRY temp;

	//Copy salFrom to temp entry
	strcpy(temp.displayName, salFrom->displayName);
	strcpy(temp.filename, salFrom->filename);
	temp.type = salFrom->type;

	//Copy salTo to salFrom
	strcpy(salFrom->displayName, salTo->displayName);
	strcpy(salFrom->filename,salTo->filename);
	salFrom->type=salTo->type;

	//Copy temp entry to salTo
	strcpy(salTo->displayName, temp.displayName);
	strcpy(salTo->filename, temp.filename);
	salTo->type=temp.type;
		
}

int FileScan()
{
	s32 itemCount=0, fileCount=0, dirCount=0;
	s32 x,a,b,startIndex=ROM_SELECTOR_DEFAULT_FOCUS+1;
	s8 text[50];
	s8 filename[SAL_MAX_PATH];
	s8 path[SAL_MAX_PATH];
	s8 ext[SAL_MAX_PATH];
	struct SAL_DIR d;

	freeRomLists();

#if 0
	PrintTile();
	PrintTitle("File Scan");
	sal_VideoPrint(8,120,"Scanning Directory...",SAL_RGB(31,31,31));
	sal_VideoFlip(1);
#endif
	if(sal_DirectoryGetItemCount(mRomDir,&itemCount)==SAL_ERROR)
	{
		MenuMessageBox(sal_LastErrorGet(),"","",MENU_MESSAGE_BOX_MODE_PAUSE);
		itemCount=0;
	}

	mRomCount=ROM_SELECTOR_ROM_START+itemCount;

	mRomList=(SAL_DIRECTORY_ENTRY*)malloc(sizeof(struct SAL_DIRECTORY_ENTRY)*mRomCount);

	//was there enough memory?
	if(mRomList == NULL)
	{
		MenuMessageBox("Could not allocate memory","Too many files","",MENU_MESSAGE_BOX_MODE_PAUSE);
		//not enough memory - try the minimum
		mRomList=(SAL_DIRECTORY_ENTRY*)malloc(sizeof(struct SAL_DIRECTORY_ENTRY)*ROM_SELECTOR_ROM_START);
		mRomCount=ROM_SELECTOR_ROM_START;
		if (mRomList == NULL)
		{
			//still no joy
			MenuMessageBox("Dude, I'm really broken now","Restart system","never do this again",MENU_MESSAGE_BOX_MODE_PAUSE);
			mRomCount = -1;
			return SAL_ERROR;
		}
	}

	//Add default items
	DefaultRomListItems();

	if (itemCount>0)
	{
		if (sal_DirectoryOpen(mRomDir, &d)==SAL_OK)
		{
			//Dir opened, now stream out details
			x=0;
			while(sal_DirectoryRead(&d, &mRomList[x+startIndex])==SAL_OK)
			{
				//Dir entry read
#if 0	
				PrintTile();
				PrintTitle("File Scan");
				sprintf(text,"Fetched item %d of %d",x, itemCount-1);
				sal_VideoPrint(8,120,text,SAL_RGB(31,31,31));
				PrintBar(228-4);
				sal_VideoPrint(0,228,mRomDir,SAL_RGB(0,0,0));
				sal_VideoFlip(1);
#endif	
				if (mRomList[x+startIndex].type == SAL_FILE_TYPE_FILE)
				{
					sal_DirectorySplitFilename(mRomList[x+startIndex].filename,path,filename,ext);
					if(
						sal_StringCompare(ext,"zip") == 0 ||
						sal_StringCompare(ext,"smc") == 0 ||
						sal_StringCompare(ext,"sfc") == 0)
					{
						fileCount++;
						x++;
					}
				}
				else
				{
					dirCount++;
					x++;
				}
				
			}
			mRomCount=ROM_SELECTOR_ROM_START+dirCount+fileCount;
			sal_DirectoryClose(&d);
		}
		else
		{
			//Failed to open dir - display error
			MenuMessageBox("Failed to open rom directory","","",MENU_MESSAGE_BOX_MODE_PAUSE);
			mRomCount=ROM_SELECTOR_DEFAULT_FOCUS;
		}

#if 0
		PrintTile();
		PrintTitle("File Scan");
		sal_VideoPrint(8,120,"Sorting items...",SAL_RGB(31,31,31));
		sal_VideoFlip(1);
#endif
		int lowIndex=0;
		//Put all directory entries at the top
		for(a=startIndex;a<startIndex+dirCount;a++)
		{
			if (mRomList[a].type == SAL_FILE_TYPE_FILE)
			{
				for(b=a+1;b<mRomCount;b++)
				{
					if (mRomList[b].type == SAL_FILE_TYPE_DIRECTORY)
					{
						SwapDirectoryEntry(&mRomList[a],&mRomList[b]);
						break;
					}
				}
			}
		}

		//Now sort directory entries
		for(a=startIndex;a<startIndex+dirCount;a++)
		{
			lowIndex=a;		
			for(b=a+1;b<startIndex+dirCount;b++)
			{
				if (sal_StringCompare(mRomList[b].displayName, mRomList[lowIndex].displayName) < 0)
				{
					//this index is lower
					lowIndex=b;
				}
			}
			//lowIndex should index next lowest value
			SwapDirectoryEntry(&mRomList[lowIndex],&mRomList[a]);
		}

		//Now sort file entries
		for(a=startIndex+dirCount;a<mRomCount;a++)
		{
			lowIndex=a;		
			for(b=a+1;b<mRomCount;b++)
			{
				if (sal_StringCompare(mRomList[b].displayName, mRomList[lowIndex].displayName) < 0)
				{
					//this index is lower
					lowIndex=b;
				}
			}
			//lowIndex should index next lowest value
			SwapDirectoryEntry(&mRomList[lowIndex],&mRomList[a]);
		}

	}

	return SAL_OK;
}

s32 UpdateRomCache()
{
	s8 filename[SAL_MAX_PATH];
	PrintTile();
	PrintTitle("CRC Lookup");
	sal_VideoPrint(8,120,"Saving cache to disk...",SAL_RGB(31,31,31));
	sal_VideoFlip(1);

	strcpy(filename,mRomDir);
	sal_DirectoryCombine(filename,"romcache.dat");
	sal_FileSave(filename, (u8*)&mRomList[0], sizeof(struct SAL_DIRECTORY_ENTRY)*(mRomCount));

	return SAL_OK;
}

s32 FileSelect()
{
	s8 text[SAL_MAX_PATH];
	s32 action=0;
	s32 smooth=0;
	u16 color=0;
	s32 i=0;
	s32 focus=ROM_SELECTOR_DEFAULT_FOCUS;
	s32 menuExit=0;
	s32 scanstart=0,scanend=0;
	u32 keys=0;
	s32 size=0, check=SAL_OK;
	
	FileScan();

	smooth=focus<<8;
	sal_InputIgnore();
	while (menuExit==0)
	{
		keys=sal_InputPollRepeat();

		// Change which rom is focused on:
		if (keys & SAL_INPUT_UP)
		{
			focus--; // Up
		}
		if (keys & SAL_INPUT_DOWN)
		{
			focus++; // Down
		}

		if (keys & INP_BUTTON_MENU_CANCEL) {
			action=0;
			menuExit=1;
		}
   
		if (keys & SAL_INPUT_LEFT || keys & SAL_INPUT_RIGHT)
		{
			if (keys & SAL_INPUT_LEFT) 
			{
				focus-=12;
				smooth=(focus<<8)-1;
			}      
			else if (keys & SAL_INPUT_RIGHT)
			{
				focus+=12;
				smooth=(focus<<8)-1;
			}
			
			if (focus>mRomCount-1) 
			{
				focus=mRomCount-1;
				smooth=(focus<<8)-1;
			}
			else if (focus<0)
			{
				focus=0;
				smooth=(focus<<8)-1;
			}
		}

		if (focus>mRomCount-1) 
		{
			focus=0;
			smooth=(focus<<8)-1;
		}
		else if (focus<0)
		{
			focus=mRomCount-1;
			smooth=(focus<<8)-1;
		}
		
		if (keys&INP_BUTTON_MENU_SELECT)
	    	{
			switch(focus)
			{
				case ROM_SELECTOR_SAVE_DEFAULT_DIR: //Save default directory
					SaveMenuOptions(mSystemDir, DEFAULT_ROM_DIR_FILENAME, DEFAULT_ROM_DIR_EXT, mRomDir, strlen(mRomDir), 1);
					break;

				case ROM_SELECTOR_MAIN_MENU: //Return to menu
					action=0;
					menuExit=1;
					break;
				
				case ROM_SELECTOR_DEFAULT_FOCUS: //blank space - do nothing
					break;
					
				default:
					// normal file or dir selected
					if (mRomList[focus].type == SAL_FILE_TYPE_DIRECTORY)
					{
						//Check for special directory names "." and ".."
						if (sal_StringCompare(mRomList[focus].filename,".") == 0)
						{
							//goto root directory

						}
						else if (sal_StringCompare(mRomList[focus].filename,"..") == 0)
						{
							// up a directory
							//Remove a directory from RomPath and rescan
							//Code below will never let you go further up than \SD Card\ on the Gizmondo
							//This is by design.
							sal_DirectoryGetParent(mRomDir);
							FileScan();
							focus=ROM_SELECTOR_DEFAULT_FOCUS; // default menu to non menu item
														// just to stop directory scan being started 
							smooth=focus<<8;
							sal_InputIgnore();
							break;
						}
						else
						{
							//go to sub directory
							sal_DirectoryCombine(mRomDir,mRomList[focus].filename);
							FileScan();
							focus=ROM_SELECTOR_DEFAULT_FOCUS; // default menu to non menu item
														// just to stop directory scan being started 
							smooth=focus<<8;
						}
					}
					else
					{
						// user has selected a rom, so load it
						strcpy(mRomName, mRomDir);
						sal_DirectoryCombine(mRomName,mRomList[focus].filename);
						mQuickSavePresent=0;  // reset any quick saves
						action=1;
						menuExit=1;
					}
					sal_InputIgnore();
					break;
			}
	    	}

		if (keys&INP_BUTTON_MENU_DELETE)
		{
			if(focus>ROM_SELECTOR_DEFAULT_FOCUS)
			{
				//delete current rom
				if (mRomList[focus].type != SAL_FILE_TYPE_DIRECTORY)
				{
					sprintf(text,"%s",mRomList[focus].displayName);

					if(MenuMessageBox("Are you sure you want to delete",text,"",MENU_MESSAGE_BOX_MODE_YESNO)==SAL_OK)
					{
						//deleterom(focus);
					}
				}
			}
		}

		// Draw screen:
		PrintTile();
		PrintTitle("Rom Select");

		smooth=smooth*7+(focus<<8); smooth>>=3;

		scanstart=focus-15;
		if (scanstart<0) scanstart=0;
		scanend = focus+15;
		if (scanend>mRomCount) scanend=mRomCount;
		
		for (i=scanstart;i<scanend;i++)
		{
			s32 x=0,y=0;
      
			y=(i<<4)-(smooth>>4);
			x=0;
			y+=112;
			if (y<=48 || y>=232) continue;
           
			if (i==focus)
			{
				color=SAL_RGB(0,0,0);
				PrintBar(y-4);
			}
			else
			{
				color=SAL_RGB(31,31,31);
			}

			 
			// Draw Directory icon if current entry is a directory
			if(mRomList[i].type == SAL_FILE_TYPE_DIRECTORY)
			{
				sprintf(text,"<%s>",mRomList[i].displayName);
				sal_VideoPrint(x,y,text,color);
			}
			else
			{
				sal_VideoPrint(x,y,mRomList[i].displayName,color);
			}

			
		}

		PrintBar(228-4);
		sal_VideoPrint(0,228,mRomDir,SAL_RGB(0,0,0));

		sal_VideoFlip(1);
		usleep(10000);
	}
	sal_InputIgnore();

	freeRomLists();

	return action;
}

static void ScanSaveStates(s8 *romname)
{
	s32 i=0;
	s8 savename[SAL_MAX_PATH];
	s8 filename[SAL_MAX_PATH];
	s8 ext[SAL_MAX_PATH];
	s8 path[SAL_MAX_PATH];

	if(!strcmp(romname,mSaveStateName)) return; // is current save state rom so exit
	
	sal_DirectorySplitFilename(romname,path,filename,ext);

	sprintf(savename,"%s.%s",filename,SAVESTATE_EXT);
  
	for(i=0;i<10;i++)
	{
		/*
		need to build a save state filename
		all saves are held in current working directory (lynxSaveStateDir)
		save filename has following format
		shortname(minus file ext) + SV + saveno ( 0 to 9 )
		*/
		sprintf(mSaveState[i].filename,"%s%d",savename,i);
		sprintf(mSaveState[i].fullFilename,"%s%s%s",mSystemDir,SAL_DIR_SEP,mSaveState[i].filename);
		if (sal_FileExists(mSaveState[i].fullFilename)==SAL_TRUE)
		{
			// we have a savestate
			mSaveState[i].inUse = 1;
		}
		else
		{
			// no save state
			mSaveState[i].inUse = 0;
		}
	}
	strcpy(mSaveStateName,romname);  // save the last scanned romname
}

static u8 *mTempState=NULL;

static
void LoadStateMem()
{
	SetSaveStateIoModeMemory(&mTempState);
	S9xUnfreezeGame("blah");
}

static 
void SaveStateMem()
{
	SetSaveStateIoModeMemory(&mTempState);

	S9xFreezeGame("blah");
}

static
void LoadStateFile(s8 *filename)
{
	SetSaveStateIoModeFile();

	S9xUnfreezeGame(filename);
}

static 
void SaveStateFile(s8 *filename)
{
	SetSaveStateIoModeFile();

	S9xFreezeGame(filename);
}

static s32 SaveStateSelect(s32 mode)
{
	s8 text[128];
	s32 action=11;
	s32 saveno=0;
	u32 keys=0;
	u16 *pixTo,*pixFrom;

	if(mRomName[0]==0)
	{
		// no rom loaded
		// display error message and exit
		return(0);
	}
	SaveStateMem();
	ScanSaveStates(mRomName);
	sal_InputIgnore();

	while (action!=0&&action!=100)
	{
		keys=sal_InputPollRepeat();

		if(keys&SAL_INPUT_UP) {saveno--; action=1;}
		if(keys&SAL_INPUT_DOWN) {saveno++; action=1;}
		if(saveno<-1) saveno=9;
		if(saveno>9) saveno=-1;
	      
		if(keys&INP_BUTTON_MENU_CANCEL) action=0; // exit
		else if((keys&INP_BUTTON_MENU_SELECT)&&(saveno==-1)) action=0; // exit
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==0)&&((action==2)||(action==5))) action=6;  // pre-save mode
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==1)&&(action==5)) action=8;  // pre-load mode
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==2)&&(action==5))
		{
			if(MenuMessageBox("Are you sure you want to delete","this save?","",MENU_MESSAGE_BOX_MODE_YESNO)==SAL_OK) action=13;  //delete slot with no preview
		}
		else if((keys&INP_BUTTON_MENU_PREVIEW_SAVESTATE)&&(action==12)) action=3;  // preview slot mode
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==1)&&(action==12)) action=8;  //load slot with no preview
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==0)&&(action==12)) action=6;  //save slot with no preview
		else if((keys&INP_BUTTON_MENU_SELECT)&&(mode==2)&&(action==12))
		{
			if(MenuMessageBox("Are you sure you want to delete","this save?","",MENU_MESSAGE_BOX_MODE_YESNO)==SAL_OK) action=13;  //delete slot with no preview
		}

		PrintTile();
		PrintTitle("Save States");
		sal_VideoPrint(12,230,"Press UP and DOWN to change save slot",SAL_RGB(31,15,5));
      
		if(saveno==-1) 
		{
			if(action!=10&&action!=0) 
			{
				action=10;
			}
		}
		else
		{
			PrintBar(60-4);
			sprintf(text,"SLOT %d",saveno);
			sal_VideoPrint(136,60,text,SAL_RGB(0,0,0));
		}
      
		switch(action)
		{
			case 1:
				//sal_VideoPrint(112,145,14,"Checking....",(unsigned short)SAL_RGB(31,31,31));
				break;
			case 2:
				sal_VideoPrint(144,145,"FREE",SAL_RGB(31,31,31));
				break;
			case 3:
				sal_VideoPrint(104,145,"Previewing....",SAL_RGB(31,31,31));
				break;
			case 4:
				sal_VideoPrint(88,145,"Previewing....fail",SAL_RGB(31,31,31));
				break;
			case 5: 
				sal_VideoBitmapScale(0, 0, SNES_WIDTH, SNES_HEIGHT, 320/2, 240/2, 320/2, &mTempFb[0], (u16*)sal_VideoGetBuffer()+(320*85)+(640-SNES_WIDTH)+16);

				if(mode==1) sal_VideoPrint((320-(strlen(MENU_TEXT_LOAD_SAVESTATE)<<3))>>1,210,MENU_TEXT_LOAD_SAVESTATE,SAL_RGB(31,31,31));
				else if(mode==0) sal_VideoPrint((320-(strlen(MENU_TEXT_OVERWRITE_SAVESTATE)<<3))>>1,210,MENU_TEXT_OVERWRITE_SAVESTATE,SAL_RGB(31,31,31));
				else if(mode==2) sal_VideoPrint((320-(strlen(MENU_TEXT_DELETE_SAVESTATE)<<3))>>1,210,MENU_TEXT_DELETE_SAVESTATE,SAL_RGB(31,31,31));
				break;
			case 6:
				sal_VideoPrint(124,145,"Saving...",SAL_RGB(31,31,31));
				break;
			case 7:
				sal_VideoPrint(124,145,"Saving...Fail!",SAL_RGB(31,31,31));
				break;
			case 8:
				sal_VideoPrint(116,145,"loading....",SAL_RGB(31,31,31));
				break;
				case 9:
				sal_VideoPrint(116,145,"loading....Fail",SAL_RGB(31,31,31));
				break;
			case 10:	
				PrintBar(145-4);
				sal_VideoPrint(104,145,"Return To Menu",SAL_RGB(0,0,0));
				break;
			case 12:
				sal_VideoPrint(124,145,"Slot used",SAL_RGB(31,31,31));
				sal_VideoPrint((320-(strlen(MENU_TEXT_PREVIEW_SAVESTATE)<<3))>>1,165,MENU_TEXT_PREVIEW_SAVESTATE,SAL_RGB(31,31,31));
				if(mode==1) sal_VideoPrint((320-(strlen(MENU_TEXT_LOAD_SAVESTATE)<<3))>>1,175,MENU_TEXT_LOAD_SAVESTATE,SAL_RGB(31,31,31));
				else if(mode==0) sal_VideoPrint((320-(strlen(MENU_TEXT_OVERWRITE_SAVESTATE)<<3))>>1,175,MENU_TEXT_OVERWRITE_SAVESTATE,SAL_RGB(31,31,31));
				else if(mode==2) sal_VideoPrint((320-(strlen(MENU_TEXT_DELETE_SAVESTATE)<<3))>>1,175,MENU_TEXT_DELETE_SAVESTATE,SAL_RGB(31,31,31));
				break;
			case 13:
				sal_VideoPrint(116,145,"Deleting....",SAL_RGB(31,31,31));
				break;
		}
      
		sal_VideoFlip(1);
      
		switch(action)
		{
			case 1:
				if(mSaveState[saveno].inUse) 
				{
					action=3;
				}
				else 
				{
					action=2;
				}
				break;
			case 3:
				LoadStateFile(mSaveState[saveno].fullFilename);
				Settings.APUEnabled = 0;
				Settings.NextAPUEnabled = Settings.APUEnabled;					
				S9xSetSoundMute (TRUE);
				GFX.Screen = (uint8 *) &mTempFb[0];
				IPPU.RenderThisFrame=TRUE;
				S9xMainLoop ();
				action=5;
				break;
			case 6:
				//Reload state in case user has been previewing
				LoadStateMem();
				SaveStateFile(mSaveState[saveno].fullFilename);
				mSaveState[saveno].inUse=1;
				action=1;
				break;
			case 7:
				action=1;
				break;
			case 8:
				LoadStateFile(mSaveState[saveno].fullFilename);
				action=100;  // loaded ok so exit
				break;
			case 9:
				action=1;
				break;
			case 11:
				action=1;
				break;
			case 13:
				sal_FileDelete(mSaveState[saveno].fullFilename);
				mSaveState[saveno].inUse = 0;
				action=1;
				break;
		}

		usleep(10000);
	}
	if (action!=100)
	{
		LoadStateMem();
	}
	if(mTempState) free(mTempState);
	mTempState=NULL;
	GFX.Screen = (uint8 *) sal_VideoGetBuffer();
	sal_InputIgnore();
	return(action);
}

static
void RenderMenu(const char *menuName, s32 menuCount, s32 menuSmooth, s32 menufocus)
{
	
	s32 i=0;
	u16 color=0;
	PrintTile();
	PrintTitle(menuName);

    	for (i=0;i<menuCount;i++)
    	{
      		int x=0,y=0;

      		y=(i<<4)-(menuSmooth>>4);
		x=8;
      		y+=112;

      		if (y<=48 || y>=232) continue;
      
      		if (i==menufocus)
      		{
        		color=SAL_RGB(0,0,0);
			PrintBar(y-4);
      		}
      		else
      		{
        		color=SAL_RGB(31,31,31);
      		}

      		sal_VideoPrint(x,y,mMenuText[i],color);
    	}
}

static
s32 SaveStateMenu(void)
{
	s32 menuExit=0,menuCount=SAVESTATE_MENU_COUNT,menufocus=0,menuSmooth=0;
	s32 action=0;
	s32 subaction=0;
	u32 keys=0;

	//Update
	strcpy(mMenuText[SAVESTATE_MENU_LOAD],"Load State");
	strcpy(mMenuText[SAVESTATE_MENU_SAVE],"Save State");
	strcpy(mMenuText[SAVESTATE_MENU_DELETE],"Delete State");
	strcpy(mMenuText[SAVESTATE_MENU_RETURN],"Back");
	sal_InputIgnore();

	while (!menuExit)
	{
		// Draw screen:
		menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
		RenderMenu("Save States", menuCount,menuSmooth,menufocus);
		sal_VideoFlip(1);

		keys=sal_InputPollRepeat();

		// Change which rom is focused on:
		if (keys & SAL_INPUT_UP) menufocus--; // Up
		if (keys & SAL_INPUT_DOWN) menufocus++; // Down
    
		if (keys&INP_BUTTON_MENU_CANCEL) menuExit=1;
    
		if (menufocus>menuCount-1)
		{
			menufocus=0;
			menuSmooth=(menufocus<<8)-1;
		}   
		else if (menufocus<0) 
		{
			menufocus=menuCount-1;
			menuSmooth=(menufocus<<8)-1;
		}

		if (keys&INP_BUTTON_MENU_SELECT)
		{
			switch(menufocus)
			{
				case SAVESTATE_MENU_LOAD:
					subaction=SaveStateSelect(SAVESTATE_MODE_LOAD);
					if(subaction==100)
					{
						menuExit=1;
						action=100;
					}
					break;
				case SAVESTATE_MENU_SAVE:
					SaveStateSelect(SAVESTATE_MODE_SAVE);
					break;
				case SAVESTATE_MENU_DELETE:
					SaveStateSelect(SAVESTATE_MODE_DELETE);
					break;
				case SAVESTATE_MENU_RETURN:
					menuExit=1;
					break;
			}	
		}

		usleep(10000);
	}
  sal_InputIgnore();
  return action;
}

void ShowCredits()
{
	s32 menuExit=0,menuCount=5,menufocus=0,menuSmooth=0;
	u32 keys=0;

	strcpy(mMenuText[0],"PocketSNES - built " __DATE__);
	strcpy(mMenuText[1],"-------------------------------------");
	strcpy(mMenuText[2],"PocketSNES created by Scott Ramsby");
	strcpy(mMenuText[3],"Initial port to the Dingoo by Reesy");
	strcpy(mMenuText[4],"Ported to OpenDingux by pcercuei");

	sal_InputIgnore();
	while (!menuExit)
	{
		keys=sal_InputPollRepeat();

		if (keys & SAL_INPUT_UP) menufocus--; // Up
		if (keys & SAL_INPUT_DOWN) menufocus++; // Down

    
		if (keys&INP_BUTTON_MENU_CANCEL) menuExit=1;
    
		if (menufocus>menuCount-1)
		{
			menufocus=0;
			menuSmooth=(menufocus<<8)-1;
		}   
		else if (menufocus<0) 
		{
			menufocus=menuCount-1;
			menuSmooth=(menufocus<<8)-1;
		}

		// Draw screen:
		menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
		RenderMenu("Credits", menuCount,menuSmooth,menufocus);
		sal_VideoFlip(1);
		usleep(10000);
	}
  	sal_InputIgnore();
}

static 
void MainMenuUpdateText(s32 menu_index)
{
	switch(menu_index)
	{
		case MENU_STATE:
			strcpy(mMenuText[MENU_STATE],"Save States");
			break;

		case MENU_RESET_GAME:
			strcpy(mMenuText[MENU_RESET_GAME],"Reset Game");
			break;

		case MENU_EXIT_APP:
			strcpy(mMenuText[MENU_EXIT_APP],"Exit Application");
			break;

		case MENU_CREDITS:
			strcpy(mMenuText[MENU_CREDITS],"Credits");
			break;

		case MENU_RETURN:
			strcpy(mMenuText[MENU_RETURN],"Return To Game");
			break;

		case MENU_AUTO_SAVE_SRAM:
			sprintf(mMenuText[MENU_AUTO_SAVE_SRAM],
						"Auto Save SRAM:             %s",
						mMenuOptions->autoSaveSram ? "ON" : "OFF");
			break;

		case MENU_TRANSPARENCY:
			sprintf(mMenuText[MENU_TRANSPARENCY],
						"Transparency:               %s",
						mMenuOptions->transparency ? "ON" : "OFF");
			break;

		case MENU_SOUND_ON:
			sprintf(mMenuText[MENU_SOUND_ON],
						"Sound:                      %s",
						mMenuOptions->soundEnabled ? "ON" : "OFF");
			break;
		
		case MENU_SOUND_RATE:		
			sprintf(mMenuText[MENU_SOUND_RATE],"Sound Rate:                 %d",mMenuOptions->soundRate);
			break;

		case MENU_SOUND_STEREO:
			sprintf(mMenuText[MENU_SOUND_STEREO],
						"Stereo:                     %s",
						mMenuOptions->stereo ? "ON" : "OFF");
			break;

#if 0
		case MENU_CPU_SPEED:		
			sprintf(mMenuText[MENU_CPU_SPEED],"Cpu Speed:                  %d",mMenuOptions->cpuSpeed);
			break;
		
		case MENU_SOUND_VOL:
			sprintf(mMenuText[MENU_SOUND_VOL],"Volume:                     %d",mMenuOptions->volume);
			break;
#endif
		
		case MENU_FRAMESKIP: 
			switch(mMenuOptions->frameSkip)
			{
				case 0:
					strcpy(mMenuText[MENU_FRAMESKIP],"Frameskip:                  AUTO");
					break;
				default:
					sprintf(mMenuText[MENU_FRAMESKIP],"Frameskip:                  %d",mMenuOptions->frameSkip-1);
					break;
			}
			break;

		case MENU_FPS:
			sprintf(mMenuText[MENU_FPS],
						"Show FPS:                   %s",
						mMenuOptions->showFps ? "ON" : "OFF");
			break;

#if 0
		case MENU_FULLSCREEN:
			switch(mMenuOptions->fullScreen)
			{
				case 0:
					strcpy(mMenuText[MENU_FULLSCREEN],"Full Screen Mode:           OFF");
					break;
				case 1:
					strcpy(mMenuText[MENU_FULLSCREEN],"Full Screen Mode:           ON");
					break;  
			}
			break;
#endif
			
		case MENU_LOAD_GLOBAL_SETTINGS:
			strcpy(mMenuText[MENU_LOAD_GLOBAL_SETTINGS],"Load Global Settings");
			break;
			
		case MENU_SAVE_GLOBAL_SETTINGS:
			strcpy(mMenuText[MENU_SAVE_GLOBAL_SETTINGS],"Save Global Settings");
			break;
			
		case MENU_LOAD_CURRENT_SETTINGS:
			strcpy(mMenuText[MENU_LOAD_CURRENT_SETTINGS],"Load Settings For Current Game");
			break;
		
		case MENU_SAVE_CURRENT_SETTINGS:
			strcpy(mMenuText[MENU_SAVE_CURRENT_SETTINGS],"Save Settings For Current Game");
			break;

		case MENU_DELETE_CURRENT_SETTINGS:
			strcpy(mMenuText[MENU_DELETE_CURRENT_SETTINGS],"Delete Settings For Current Game");
			break;

		case MENU_SAVE_SRAM:
			strcpy(mMenuText[MENU_SAVE_SRAM],"Save SRAM");
			break;

		case MENU_ROM_SELECT:
			strcpy(mMenuText[MENU_ROM_SELECT],"Select Rom");
			break;
	}
}

static
void MainMenuUpdateTextAll(void)
{
	MainMenuUpdateText(MENU_STATE);
	MainMenuUpdateText(MENU_RESET_GAME);
	MainMenuUpdateText(MENU_EXIT_APP);
	MainMenuUpdateText(MENU_RETURN);
	MainMenuUpdateText(MENU_TRANSPARENCY);
//	MainMenuUpdateText(MENU_CPU_SPEED);
	MainMenuUpdateText(MENU_SOUND_ON);
	MainMenuUpdateText(MENU_SOUND_STEREO);
	MainMenuUpdateText(MENU_SOUND_RATE);
//	MainMenuUpdateText(MENU_SOUND_VOL);
	MainMenuUpdateText(MENU_FRAMESKIP);
	MainMenuUpdateText(MENU_FPS);
//	MainMenuUpdateText(MENU_FULLSCREEN);
	MainMenuUpdateText(MENU_LOAD_GLOBAL_SETTINGS);
	MainMenuUpdateText(MENU_SAVE_GLOBAL_SETTINGS);
	MainMenuUpdateText(MENU_LOAD_CURRENT_SETTINGS);
	MainMenuUpdateText(MENU_SAVE_CURRENT_SETTINGS);
	MainMenuUpdateText(MENU_DELETE_CURRENT_SETTINGS);
	MainMenuUpdateText(MENU_RETURN);
	MainMenuUpdateText(MENU_CREDITS);
	MainMenuUpdateText(MENU_AUTO_SAVE_SRAM);
	MainMenuUpdateText(MENU_SAVE_SRAM);
	MainMenuUpdateText(MENU_ROM_SELECT);
}

void MenuReloadOptions()
{
	if(mRomName[0]!=0)
	{
		//Load settings for game
		if (LoadMenuOptions(mSystemDir, mRomName, MENU_OPTIONS_EXT, (s8*)mMenuOptions, sizeof(struct MENU_OPTIONS), 0) == SAL_OK)
		{
			return;
		}
	}

	//Load global settings
	if(LoadMenuOptions(mSystemDir, MENU_OPTIONS_FILENAME, MENU_OPTIONS_EXT, (s8*)mMenuOptions, sizeof(struct MENU_OPTIONS), 0) == SAL_OK)
	{
		return;
	}

	DefaultMenuOptions();
}

void MenuInit(const char *systemDir, struct MENU_OPTIONS *menuOptions)
{
	s8 filename[SAL_MAX_PATH];
	u16 *pix;
	s32 x;
	
	strcpy(mSystemDir,systemDir);
	mMenuOptions=menuOptions;

	if(LoadMenuOptions(mSystemDir, DEFAULT_ROM_DIR_FILENAME, DEFAULT_ROM_DIR_EXT, mRomDir, SAL_MAX_PATH, 0)!=SAL_OK)
	{
		strcpy(mRomDir,systemDir);
	}

	pix=&mMenuTile[0];
	for(x=0;x<64*64;x++) *pix++=0;
	pix=&mMenuHeader[0];
	for(x=0;x<320*48;x++) *pix++=0;
	pix=&mHighLightBar[0];
	for(x=0;x<320*16;x++) *pix++=0xFFFF;

	sal_ImageLoad("pocketsnes_tile.png", &mMenuTile, 64, 64);
	sal_ImageLoad("pocketsnes_header.png", &mMenuHeader, 320, 48);
	sal_ImageLoad("pocketsnes_bar.png", &mHighLightBar, 320, 16);

	MenuReloadOptions();
}



extern "C" void S9xSaveSRAM(int showWarning);

s32 MenuRun(s8 *romName)
{
	s32 menuExit=0,menuCount=MENU_COUNT,menufocus=0,menuSmooth=0;
	s32 action=EVENT_NONE;
	s32 subaction=0;
	u32 keys=0;

	sal_CpuSpeedSet(MENU_NORMAL_CPU_SPEED);

	sal_VideoInit(16,0,60);
	sal_VideoSetScaling(320,240);

	if(sal_StringCompare(mRomName,romName)!=0)
	{
		action=EVENT_LOAD_ROM;
		strcpy(mRomName,romName);
		return action;
	}

	if((mMenuOptions->autoSaveSram) && (mRomName[0]!=0))
	{
		MenuMessageBox("Saving SRAM...","","",MENU_MESSAGE_BOX_MODE_MSG);
		S9xSaveSRAM(0);
	}

	MainMenuUpdateTextAll();
	sal_InputIgnore();

	while (!menuExit)
	{
		// Draw screen:
		menuSmooth=menuSmooth*7+(menufocus<<8); menuSmooth>>=3;
		RenderMenu("Main Menu", menuCount,menuSmooth,menufocus);
		sal_VideoFlip(1);

		keys=sal_InputPollRepeat();

		if (keys & SAL_INPUT_UP) menufocus--; // Up
		if (keys & SAL_INPUT_DOWN) menufocus++; // Down
    
		if (keys&INP_BUTTON_MENU_CANCEL) 
		{
			if(mRomName[0]!=0)
			{
				action=EVENT_RUN_ROM;
				menuExit=1;
			}
		}
    
		if (menufocus>menuCount-1)
		{
			menufocus=0;
			menuSmooth=(menufocus<<8)-1;
		}   
		else if (menufocus<0) 
		{
			menufocus=menuCount-1;
			menuSmooth=(menufocus<<8)-1;
		}

		if (keys & SAL_INPUT_LEFT||
			keys & SAL_INPUT_RIGHT)
		{
			switch(menufocus)
			{
				case MENU_SOUND_ON:
					mMenuOptions->soundEnabled^=1;
					MainMenuUpdateText(MENU_SOUND_ON);
					break;

				case MENU_SOUND_STEREO:
					mMenuOptions->stereo^=1;
					MainMenuUpdateText(MENU_SOUND_STEREO);
					break;

				case MENU_TRANSPARENCY:
					mMenuOptions->transparency^=1;
					MainMenuUpdateText(MENU_TRANSPARENCY);
					break;

				case MENU_AUTO_SAVE_SRAM:
					mMenuOptions->autoSaveSram^=1;
					MainMenuUpdateText(MENU_AUTO_SAVE_SRAM);
					break;

#if 0
				case MENU_CPU_SPEED:
					
					if (keys & SAL_INPUT_RIGHT)
					{
						if(keys&INP_BUTTON_MENU_SELECT)
						{
							mMenuOptions->cpuSpeed=sal_CpuSpeedNextFast(mMenuOptions->cpuSpeed);
						}
						else
						{
							mMenuOptions->cpuSpeed=sal_CpuSpeedNext(mMenuOptions->cpuSpeed);
						}	
					}
					else
					{
						if(keys&INP_BUTTON_MENU_SELECT)
						{
							mMenuOptions->cpuSpeed=sal_CpuSpeedPreviousFast(mMenuOptions->cpuSpeed);
						}
						else
						{
							mMenuOptions->cpuSpeed=sal_CpuSpeedPrevious(mMenuOptions->cpuSpeed);
						}
					}
					MainMenuUpdateText(MENU_CPU_SPEED);
					break;
#endif

				case MENU_SOUND_RATE:
					if (keys & SAL_INPUT_RIGHT)
					{
						mMenuOptions->soundRate=sal_AudioRateNext(mMenuOptions->soundRate);	
					}
					else
					{
						mMenuOptions->soundRate=sal_AudioRatePrevious(mMenuOptions->soundRate);
					}
					MainMenuUpdateText(MENU_SOUND_RATE);
					break;

#if 0
				case MENU_SOUND_VOL:
					if (keys & SAL_INPUT_RIGHT)
					{
						mMenuOptions->volume+=1;
						if(mMenuOptions->volume>31) mMenuOptions->volume=0;
					}
					else
					{
						mMenuOptions->volume-=1;
						if(mMenuOptions->volume>31) mMenuOptions->volume=31;

					}
					MainMenuUpdateText(MENU_SOUND_VOL);
					break;
#endif

				case MENU_FRAMESKIP:
					if (keys & SAL_INPUT_RIGHT)
					{
						mMenuOptions->frameSkip++;
						if(mMenuOptions->frameSkip>6) mMenuOptions->frameSkip=0;
					}
					else
					{
						mMenuOptions->frameSkip--;
						if(mMenuOptions->frameSkip>6) mMenuOptions->frameSkip=6;
					}
					MainMenuUpdateText(MENU_FRAMESKIP);
					break;

				case MENU_FPS:
					mMenuOptions->showFps^=1;
					MainMenuUpdateText(MENU_FPS);
					break;

#if 0
				case MENU_FULLSCREEN:
					mMenuOptions->fullScreen^=1;
					MainMenuUpdateText(MENU_FULLSCREEN);
					break;
#endif
			}
		}

		if (keys&INP_BUTTON_MENU_SELECT)
		{
			switch(menufocus)
			{
				case MENU_ROM_SELECT:
					subaction=FileSelect();
					if (subaction==1)
					{
						action=EVENT_LOAD_ROM;
						strcpy(romName,mRomName);
//						MenuReloadOptions();
						menuExit=1;
					}
					break;
				case MENU_LOAD_GLOBAL_SETTINGS:
					LoadMenuOptions(mSystemDir, MENU_OPTIONS_FILENAME, MENU_OPTIONS_EXT, (char*)mMenuOptions, sizeof(struct MENU_OPTIONS), 1);
					MainMenuUpdateTextAll();
					break;
				case MENU_SAVE_GLOBAL_SETTINGS:
					SaveMenuOptions(mSystemDir, MENU_OPTIONS_FILENAME, MENU_OPTIONS_EXT, (char*)mMenuOptions, sizeof(struct MENU_OPTIONS), 1);
					break;

				case MENU_LOAD_CURRENT_SETTINGS:
					if(mRomName[0]!=0)
					{
						LoadMenuOptions(mSystemDir, mRomName, MENU_OPTIONS_EXT, (char*)mMenuOptions, sizeof(struct MENU_OPTIONS), 1);

						MainMenuUpdateTextAll();
					}
					break;
				case MENU_SAVE_CURRENT_SETTINGS:
					if(mRomName[0]!=0)
					{
						SaveMenuOptions(mSystemDir, mRomName, MENU_OPTIONS_EXT, (char*)mMenuOptions, sizeof(struct MENU_OPTIONS), 1);
					}
					break;

				case MENU_DELETE_CURRENT_SETTINGS:
					if(mRomName[0]!=0)
					{
						DeleteMenuOptions(mSystemDir, mRomName, MENU_OPTIONS_EXT, 1);
					}
					break;

				case MENU_STATE:
					if(mRomName[0]!=0)
					{
						subaction=SaveStateMenu();
						if (subaction==100)
						{
							action=EVENT_RUN_ROM;
							menuExit=1;
						}
					}
					MainMenuUpdateTextAll();
					break;

				case MENU_SAVE_SRAM:
					if(mRomName[0]!=0)
					{
						MenuMessageBox("","","Saving SRAM...",MENU_MESSAGE_BOX_MODE_MSG);
						S9xSaveSRAM(1);
					}
					break;

				case MENU_CREDITS:
					ShowCredits();
					MainMenuUpdateTextAll();
					break;

				case MENU_RESET_GAME:
					if(mRomName[0]!=0)
					{
						action=EVENT_RESET_ROM;
						menuExit=1;
					}
					break;
				case MENU_RETURN:
					if(mRomName[0]!=0)
					{
						action=EVENT_RUN_ROM;
						menuExit=1;
					}
					break;
				case MENU_EXIT_APP:
					action=EVENT_EXIT_APP;
					menuExit=1;
					break;
				
			}	
		}

		usleep(10000);
	}
	
  sal_InputIgnore();

  return action;
}



