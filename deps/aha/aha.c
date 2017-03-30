/*
 The contents of this file are subject to the Mozilla Public License
 Version 1.1 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 License for the specific language governing rights and limitations
 under the License.

 Alternatively, the contents of this file may be used under the terms
 of the GNU Lesser General Public license version 2 or later (LGPL2+),
 in which case the provisions of LGPL License are applicable instead of
 those above.

 For feedback and questions about my Files and Projects please mail me,
 Alexander Matthes (Ziz) , ziz_at_mailbox.org
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern void unmangle_ansi(char *body, int len, char **body_out, int *body_len);

typedef struct selem *pelem;
typedef struct selem {
	unsigned char digit[8];
	unsigned char digitcount;
	pelem next;
} telem;

pelem parseInsert(char* s)
{
	pelem firstelem=NULL;
	pelem momelem=NULL;
	unsigned char digit[8];
	unsigned char digitcount=0;
	int pos=0;
	for (pos=0;pos<1024;pos++)
	{
		if (s[pos]=='[')
			continue;
		if (s[pos]==';' || s[pos]==0)
		{
			if (digitcount==0)
			{
				digit[0]=0;
				digitcount=1;
			}

			pelem newelem=(pelem)malloc(sizeof(telem));
			for (unsigned char a=0;a<8;a++)
				newelem->digit[a]=digit[a];
			newelem->digitcount=digitcount;
			newelem->next=NULL;
			if (momelem==NULL)
				firstelem=newelem;
			else
				momelem->next=newelem;
			momelem=newelem;
			digitcount=0;
			memset(digit,0,8);
			if (s[pos]==0)
				break;
		}
		else
		if (digitcount<8)
		{
			digit[digitcount]=s[pos]-'0';
			digitcount++;
		}
	}
	return firstelem;
}

void deleteParse(pelem elem)
{
	while (elem!=NULL)
	{
		pelem temp=elem->next;
		free(elem);
		elem=temp;
	}
}

void append_output(char **output, char *stuff, int *size, int *at) {
    while (*at + strlen(stuff) + 1 >= *size) {
        *size += 256;
        *output = realloc(*output, *size);
    }

    strcat(*output, stuff);
    *at += strlen(stuff);
}

char * aha(char *input)
{
	//Searching Parameters
    char *unmangle_out;
    int unmangle_out_len;
    unmangle_ansi(input, strlen(input), &unmangle_out, &unmangle_out_len);

    unmangle_out[unmangle_out_len] = '\0';

	//Begin of Conversion
	unsigned int c;
	int fc = -1; //Standard Foreground Color //IRC-Color+8
	int bc = -1; //Standard Background Color //IRC-Color+8
	int ul = 0; //Not underlined
	int bo = 0; //Not bold
	int bl = 0; //No Blinking
	int ofc,obc,oul,obo,obl; //old values
	int line=0;
	int momline=0;
	int newline=-1;
	int temp;
    char *ptr = unmangle_out;
    char *output = (char *)malloc(256);
    int size = 256;
    int outat = 0;
    char minibuf[2];


    memset(output, 0, 256);

	while (*ptr != '\0')
	{
        c = *ptr++;
		if (c=='\033')
		{
			//Saving old values
			ofc=fc;
			obc=bc;
			oul=ul;
			obo=bo;
			obl=bl;
			//Searching the end (a letter) and safe the insert:
			c= *ptr++;
            if (c == '\0') {
                return output;
            }
			if ( c == '[' ) // CSI code, see https://en.wikipedia.org/wiki/ANSI_escape_code#Colors
			{
				char buffer[1024];
				buffer[0] = '[';
				int counter=1;
				while ((c<'A') || ((c>'Z') && (c<'a')) || (c>'z'))
				{
					c=*ptr++;
                    if (c == '\0') {
                        return output;
                    }
					buffer[counter]=c;
					if (c=='>') //end of htop
						break;
					counter++;
					if (counter>1022)
						break;
				}
				buffer[counter-1]=0;
				pelem elem;
				switch (c)
				{
					case 'm':
						//printf("\n%s\n",buffer); //DEBUG
						elem=parseInsert(buffer);
						pelem momelem=elem;
						while (momelem!=NULL)
						{
							//jump over zeros
							int mompos=0;
							while (mompos<momelem->digitcount && momelem->digit[mompos]==0)
								mompos++;
							if (mompos==momelem->digitcount) //only zeros => delete all
							{
								bo=0;ul=0;bl=0;fc=-1;bc=-1;
							}
							else
							{
								switch (momelem->digit[mompos])
								{
									case 1: if (mompos+1==momelem->digitcount)  // 1, 1X not supported
												bo=1;
											break;
									case 2: if (mompos+1<momelem->digitcount) // 2X, 2 not supported
												switch (momelem->digit[mompos+1])
												{
													case 1: //Reset and double underline (which aha doesn't support)
													case 2: //Reset bold
														bo=0;
														break;
													case 4: //Reset underline
														ul=0;
														break;
													case 5: //Reset blink
														bl=0;
														break;
													case 7: //Reset Inverted
														if (bc == -1)
															bc = 8;
														if (fc == -1)
															fc = 9;
														temp = bc;
														bc = fc;
														fc = temp;
														break;
												}
											break;
									case 3: if (mompos+1<momelem->digitcount)  // 3X, 3 not supported
												fc=momelem->digit[mompos+1];
											break;
									case 4: if (mompos+1==momelem->digitcount)  // 4
												ul=1;
											else // 4X
												bc=momelem->digit[mompos+1];
											break;
									case 5: if (mompos+1==momelem->digitcount) //5, 5X not supported
												bl=1;
											break;
									//6 and 6X not supported at all
									case 7: if (bc == -1) //7, 7X is mot defined (and supported)
												bc = 8;
											if (fc == -1)
												fc = 9;
											temp = bc;
											bc = fc;
											fc = temp;
											break;
									//8 and 9 not supported
								}
							}
							momelem=momelem->next;
						}
						deleteParse(elem);
					break;
				}
				//Checking the differences
				if ((fc!=ofc) || (bc!=obc) || (ul!=oul) || (bo!=obo) || (bl!=obl)) //ANY Change
				{
					if ((ofc!=-1) || (obc!=-1) || (oul!=0) || (obo!=0) || (obl!=0))
						append_output(&output, "</span>", &size, &outat);
					if ((fc!=-1) || (bc!=-1) || (ul!=0) || (bo!=0) || (bl!=0))
					{
                        append_output(&output, "<span style=\"", &size, &outat);
						switch (fc)
						{
							case	0: 
                                             if (bo) {
                                                 append_output(&output, "color:dimgray;", &size, &outat);
                                             } else {
                                                 append_output(&output, "color:dimgray;", &size, &outat);
                                             }
											 break; //Black
							case	1: 
                                             if (bo) {
                                                append_output(&output, "color:#FF8888;", &size, &outat);
                                             } else {
                                                append_output(&output, "color:red;", &size, &outat);
                                             }
											 break; //Red
							case	2: 
                                             if (bo) {
                                                 append_output(&output, "color:lime;", &size, &outat);
                                             } else {
                                                 append_output(&output, "color:#00ff00;", &size, &outat);
                                             }
											 break; //Green
							case	3: 
                                            if (bo) {
                                                 append_output(&output, "color:yellow;", &size, &outat);
                                            } else {
                                                 append_output(&output, "color:olive;", &size, &outat);
                                            }
											 break; //Yellow
							case	4: 
                                            if (bo) {
                                                 append_output(&output, "color:#8888FF;", &size, &outat);
                                            } else {
                                                 append_output(&output, "color:#0000FF;", &size, &outat);
                                            }
											 break; //Blue
							case	5: 
                                            if (bo) {
                                                 append_output(&output, "color:fuchsia;", &size, &outat);
                                            } else {
                                                 append_output(&output, "color:#FF00FF;", &size, &outat);
                                            }
											 break; //Purple
							case	6: 
                                             if (bo) {
                                                 append_output(&output, "color:aqua;", &size, &outat);
                                             } else {
                                                 append_output(&output, "color:#FFFF00", &size, &outat);
                                             }
											 break; //Cyan
							case	7: 
                                            if (bo) {
                                                 append_output(&output, "color:white;", &size, &outat);
                                            } else {
                                                 append_output(&output, "color:grey;", &size, &outat);
                                            }
											 break; //White
							case	8: 
                                             append_output(&output, "color:black;", &size, &outat);
											 break; //Background Colour
							case	9: 
                                             append_output(&output, "color:white;", &size, &outat);
											 break; //Foreground Color
						}
						switch (bc)
						{
							case	0: 
                                             append_output(&output, "background-color:black;", &size, &outat);
											 break; //Black
							case	1: 
											 append_output(&output, "background-color:red;", &size, &outat);
											 break; //Red
							case	2: 
                                			 
                                             append_output(&output, "background-color:lime;", &size, &outat);
											 break; //Green
							case	3: 
                                             append_output(&output, "background-color:yellow;", &size, &outat);
											 break; //Yellow
							case	4: 
                                             append_output(&output, "background-color:#3333FF;", &size, &outat);
											 break; //Blue
							case	5: 
                                             append_output(&output, "background-color:fuchsia;", &size, &outat);
											 break; //Purple
							case	6: 
                                             append_output(&output, "background-color:aqua;", &size, &outat);
											 break; //Cyan
							case	7: 
                                             append_output(&output, "background-color:white;", &size, &outat);
											 break; //White
							case	8: 
                                             append_output(&output, "background-color:black;", &size, &outat);
											 break; //Background Colour
							case	9: 
                                             append_output(&output, "background-color:white;", &size, &outat);
											 break; //Foreground Colour
						}
						if (ul)
						{
                            append_output(&output, "text-decoration:underline;", &size, &outat);
						}
						if (bl)
						{
                            append_output(&output, "text-decoration:blink;", &size, &outat);
						}

                        append_output(&output, "\">", &size, &outat);
					}
				}
			}
		}
		else
		if (c==13)
		{
			for (;line<80;line++)

               append_output(&output, "&nbsp;", &size, &outat);
			line=0;
			momline++;
			append_output(&output, "<br />\n", &size, &outat);
		}
		else if (c!=8)
		{
			line++;
			if (newline>=0)
			{
				while (newline>line)
				{
                    append_output(&output, " ", &size, &outat);
					line++;
				}
				newline=-1;
			}
			switch (c)
			{
                 case '\x01' : append_output(&output, "&#x263A;", &size, &outat); break;
                 case '\x02' : append_output(&output, "&#x263B;", &size, &outat); break;
                 case '\x03' : append_output(&output, "&#x2665;", &size, &outat); break;
                 case '\x04' : append_output(&output, "&#x2666;", &size, &outat); break;
                 case '\x05' : append_output(&output, "&#x2663;", &size, &outat); break;
                 case '\x06' : append_output(&output, "&#x2660;", &size, &outat); break;
                 case '\x07' : append_output(&output, "&#x2022;", &size, &outat); break;
                 case '\x08' : append_output(&output, "&#x25D8;", &size, &outat); break;
                 case '\x09' : append_output(&output, "&#x25CB;", &size, &outat); break;
                 //case '\x0a' : append_output(&output, "&#x25D8;", &size, &outat); break;
                 case '\x0b' : append_output(&output, "&#x2642;", &size, &outat); break;
                 case '\x0c' : append_output(&output, "&#x2640;", &size, &outat); break;
                 //case '\x0d' : append_output(&output, "&#x266A;", &size, &outat); break;
                 case '\x0e' : append_output(&output, "&#x266B;", &size, &outat); break;
                 case '\x0f' : append_output(&output, "&#x263C;", &size, &outat); break;
                 case '\x10' : append_output(&output, "&#x25B8;", &size, &outat); break;
                 case '\x11' : append_output(&output, "&#x25C2;", &size, &outat); break;
                 case '\x12' : append_output(&output, "&#x2195;", &size, &outat); break;
                 case '\x13' : append_output(&output, "&#x203C;", &size, &outat); break;
                 case '\x14' : append_output(&output, "&#x00B6;", &size, &outat); break;
                 case '\x15' : append_output(&output, "&#x00A7;", &size, &outat); break;
                 case '\x16' : append_output(&output, "&#x25AC;", &size, &outat); break;
                 case '\x17' : append_output(&output, "&#x21A8;", &size, &outat); break;
                 case '\x18' : append_output(&output, "&#x2191;", &size, &outat); break;
                 case '\x19' : append_output(&output, "&#x2193;", &size, &outat); break;
                 case '\x1a' : append_output(&output, "&#x2192;", &size, &outat); break;
                 case '\x1b' : append_output(&output, "&#x2190;", &size, &outat); break;
                 case '\x1c' : append_output(&output, "&#x221F;", &size, &outat); break;
                 case '\x1d' : append_output(&output, "&#x2194;", &size, &outat); break;
                 case '\x1e' : append_output(&output, "&#x25B4;", &size, &outat); break;
                 case '\x1f' : append_output(&output, "&#x25BE;", &size, &outat); break;
                 case '\x21' : append_output(&output, "&#x0021;", &size, &outat); break;
                 case '\x22' : append_output(&output, "&#x0022;", &size, &outat); break;
                 case '\x23' : append_output(&output, "&#x0023;", &size, &outat); break;
                 case '\x24' : append_output(&output, "&#x0024;", &size, &outat); break;
                 case '\x25' : append_output(&output, "&#x0025;", &size, &outat); break;
                 case '\x26' : append_output(&output, "&#x0026;", &size, &outat); break;
                 case '\x27' : append_output(&output, "&#x0027;", &size, &outat); break;
                 case '\x28' : append_output(&output, "&#x0028;", &size, &outat); break;
                 case '\x29' : append_output(&output, "&#x0029;", &size, &outat); break;
                 case '\x2a' : append_output(&output, "&#x002A;", &size, &outat); break;
                 case '\x2b' : append_output(&output, "&#x002B;", &size, &outat); break;
                 case '\x2c' : append_output(&output, "&#x002C;", &size, &outat); break;
                 case '\x2d' : append_output(&output, "&#x002D;", &size, &outat); break;
                 case '\x2e' : append_output(&output, "&#x002E;", &size, &outat); break;
                 case '\x2f' : append_output(&output, "&#x002F;", &size, &outat); break;
                 case '\x7f' : append_output(&output, "&#x2302;", &size, &outat); break;
                 case '\x80' : append_output(&output, "&#x00C7;", &size, &outat); break;
                 case '\x81' : append_output(&output, "&#x00FC;", &size, &outat); break;
                 case '\x82' : append_output(&output, "&#x00E9;", &size, &outat); break;
                 case '\x83' : append_output(&output, "&#x00E2;", &size, &outat); break;
                 case '\x84' : append_output(&output, "&#x00E4;", &size, &outat); break;
                 case '\x85' : append_output(&output, "&#x00E0;", &size, &outat); break;
                 case '\x86' : append_output(&output, "&#x00E5;", &size, &outat); break;
                 case '\x87' : append_output(&output, "&#x00E7;", &size, &outat); break;
                 case '\x88' : append_output(&output, "&#x00EA;", &size, &outat); break;
                 case '\x89' : append_output(&output, "&#x00EB;", &size, &outat); break;
                 case '\x8a' : append_output(&output, "&#x00E8;", &size, &outat); break;
                 case '\x8b' : append_output(&output, "&#x00EF;", &size, &outat); break;
                 case '\x8c' : append_output(&output, "&#x00EE;", &size, &outat); break;
                 case '\x8d' : append_output(&output, "&#x00EC;", &size, &outat); break;
                 case '\x8e' : append_output(&output, "&#x00C4;", &size, &outat); break;
                 case '\x8f' : append_output(&output, "&#x00C5;", &size, &outat); break;
                 case '\x90' : append_output(&output, "&#x00C9;", &size, &outat); break;
                 case '\x91' : append_output(&output, "&#x00E6;", &size, &outat); break;
                 case '\x92' : append_output(&output, "&#x00C6;", &size, &outat); break;
                 case '\x93' : append_output(&output, "&#x00F4;", &size, &outat); break;
                 case '\x94' : append_output(&output, "&#x00F6;", &size, &outat); break;
                 case '\x95' : append_output(&output, "&#x00F2;", &size, &outat); break;
                 case '\x96' : append_output(&output, "&#x00FB;", &size, &outat); break;
                 case '\x97' : append_output(&output, "&#x00F9;", &size, &outat); break;
                 case '\x98' : append_output(&output, "&#x00FF;", &size, &outat); break;
                 case '\x99' : append_output(&output, "&#x00D6;", &size, &outat); break;
                 case '\x9a' : append_output(&output, "&#x00DC;", &size, &outat); break;
                 case '\x9b' : append_output(&output, "&#x00A2;", &size, &outat); break;
                 case '\x9c' : append_output(&output, "&#x00A3;", &size, &outat); break;
                 case '\x9d' : append_output(&output, "&#x00A5;", &size, &outat); break;
                 case '\x9e' : append_output(&output, "&#x20A7;", &size, &outat); break;
                 case '\x9f' : append_output(&output, "&#x0192;", &size, &outat); break;
                 case '\xa0' : append_output(&output, "&#x00E1;", &size, &outat); break;
                 case '\xa1' : append_output(&output, "&#x00ED;", &size, &outat); break;
                 case '\xa2' : append_output(&output, "&#x00F3;", &size, &outat); break;
                 case '\xa3' : append_output(&output, "&#x00FA;", &size, &outat); break;
                 case '\xa4' : append_output(&output, "&#x00F1;", &size, &outat); break;
                 case '\xa5' : append_output(&output, "&#x00D1;", &size, &outat); break;
                 case '\xa6' : append_output(&output, "&#x00AA;", &size, &outat); break;
                 case '\xa7' : append_output(&output, "&#x00BA;", &size, &outat); break;
                 case '\xa8' : append_output(&output, "&#x00BF;", &size, &outat); break;
                 case '\xa9' : append_output(&output, "&#x2310;", &size, &outat); break;
                 case '\xaa' : append_output(&output, "&#x00AC;", &size, &outat); break;
                 case '\xab' : append_output(&output, "&#x00BD;", &size, &outat); break;
                 case '\xac' : append_output(&output, "&#x00BC;", &size, &outat); break;
                 case '\xad' : append_output(&output, "&#x00A1;", &size, &outat); break;
                 case '\xae' : append_output(&output, "&#x00AB;", &size, &outat); break;
                 case '\xaf' : append_output(&output, "&#x00BB;", &size, &outat); break;
                 case '\xb0' : append_output(&output, "&#x2591;", &size, &outat); break;
                 case '\xb1' : append_output(&output, "&#x2592;", &size, &outat); break;
                 case '\xb2' : append_output(&output, "&#x2593;", &size, &outat); break;
                 case '\xb3' : append_output(&output, "&#x2502;", &size, &outat); break;
                 case '\xb4' : append_output(&output, "&#x2524;", &size, &outat); break;
                 case '\xb5' : append_output(&output, "&#x2561;", &size, &outat); break;
                 case '\xb6' : append_output(&output, "&#x2562;", &size, &outat); break;
                 case '\xb7' : append_output(&output, "&#x2556;", &size, &outat); break;
                 case '\xb8' : append_output(&output, "&#x2555;", &size, &outat); break;
                 case '\xb9' : append_output(&output, "&#x2563;", &size, &outat); break;
                 case '\xba' : append_output(&output, "&#x2551;", &size, &outat); break;
                 case '\xbb' : append_output(&output, "&#x2557;", &size, &outat); break;
                 case '\xbc' : append_output(&output, "&#x255D;", &size, &outat); break;
                 case '\xbd' : append_output(&output, "&#x255C;", &size, &outat); break;
                 case '\xbe' : append_output(&output, "&#x255B;", &size, &outat); break;
                 case '\xbf' : append_output(&output, "&#x2510;", &size, &outat); break;
                 case '\xc0' : append_output(&output, "&#x2514;", &size, &outat); break;
                 case '\xc1' : append_output(&output, "&#x2534;", &size, &outat); break;
                 case '\xc2' : append_output(&output, "&#x252C;", &size, &outat); break;
                 case '\xc3' : append_output(&output, "&#x251C;", &size, &outat); break;
                 case '\xc4' : append_output(&output, "&#x2500;", &size, &outat); break;
                 case '\xc5' : append_output(&output, "&#x253C;", &size, &outat); break;
                 case '\xc6' : append_output(&output, "&#x255E;", &size, &outat); break;
                 case '\xc7' : append_output(&output, "&#x255F;", &size, &outat); break;
                 case '\xc8' : append_output(&output, "&#x255A;", &size, &outat); break;
                 case '\xc9' : append_output(&output, "&#x2554;", &size, &outat); break;
                 case '\xca' : append_output(&output, "&#x2569;", &size, &outat); break;
                 case '\xcb' : append_output(&output, "&#x2566;", &size, &outat); break;
                 case '\xcc' : append_output(&output, "&#x2560;", &size, &outat); break;
                 case '\xcd' : append_output(&output, "&#x2550;", &size, &outat); break;
                 case '\xce' : append_output(&output, "&#x256C;", &size, &outat); break;
                 case '\xcf' : append_output(&output, "&#x2567;", &size, &outat); break;
                 case '\xd0' : append_output(&output, "&#x2568;", &size, &outat); break;
                 case '\xd1' : append_output(&output, "&#x2564;", &size, &outat); break;
                 case '\xd2' : append_output(&output, "&#x2565;", &size, &outat); break;
                 case '\xd3' : append_output(&output, "&#x2559;", &size, &outat); break;
                 case '\xd4' : append_output(&output, "&#x255B;", &size, &outat); break;
                 case '\xd5' : append_output(&output, "&#x2552;", &size, &outat); break;
                 case '\xd6' : append_output(&output, "&#x2553;", &size, &outat); break;
                 case '\xd7' : append_output(&output, "&#x256B;", &size, &outat); break;
                 case '\xd8' : append_output(&output, "&#x256A;", &size, &outat); break;
                 case '\xd9' : append_output(&output, "&#x2518;", &size, &outat); break;
                 case '\xda' : append_output(&output, "&#x250C;", &size, &outat); break;
                 case '\xdb' : append_output(&output, "&#x2588;", &size, &outat); break;
                 case '\xdc' : append_output(&output, "&#x2584;", &size, &outat); break;
                 case '\xdd' : append_output(&output, "&#x25BC;", &size, &outat); break;
                 case '\xde' : append_output(&output, "&#x2590;", &size, &outat); break;
                 case '\xdf' : append_output(&output, "&#x2580;", &size, &outat); break;
                 case '\xe0' : append_output(&output, "&#x03B1;", &size, &outat); break;
                 case '\xe1' : append_output(&output, "&#x03B2;", &size, &outat); break;
                 case '\xe2' : append_output(&output, "&#x0393;", &size, &outat); break;
                 case '\xe3' : append_output(&output, "&#x03C0;", &size, &outat); break;
                 case '\xe4' : append_output(&output, "&#x03A3;", &size, &outat); break;
                 case '\xe5' : append_output(&output, "&#x03C3;", &size, &outat); break;
                 case '\xe6' : append_output(&output, "&#x00B5;", &size, &outat); break;
                 case '\xe7' : append_output(&output, "&#x03C4;", &size, &outat); break;
                 case '\xe8' : append_output(&output, "&#x03A6;", &size, &outat); break;
                 case '\xe9' : append_output(&output, "&#x0398;", &size, &outat); break;
                 case '\xea' : append_output(&output, "&#x03A9;", &size, &outat); break;
                 case '\xeb' : append_output(&output, "&#x03B4;", &size, &outat); break;
                 case '\xec' : append_output(&output, "&#x221E;", &size, &outat); break;
                 case '\xed' : append_output(&output, "&#x2205;", &size, &outat); break;
                 case '\xee' : append_output(&output, "&#x2208;", &size, &outat); break;
                 case '\xef' : append_output(&output, "&#x2229;", &size, &outat); break;
                 case '\xf0' : append_output(&output, "&#x2261;", &size, &outat); break;
                 case '\xf1' : append_output(&output, "&#x00B1;", &size, &outat); break;
                 case '\xf2' : append_output(&output, "&#x2265;", &size, &outat); break;
                 case '\xf3' : append_output(&output, "&#x2264;", &size, &outat); break;
                 case '\xf4' : append_output(&output, "&#x2320;", &size, &outat); break;
                 case '\xf5' : append_output(&output, "&#x2321;", &size, &outat); break;
                 case '\xf6' : append_output(&output, "&#x00F7;", &size, &outat); break;
                 case '\xf7' : append_output(&output, "&#x2248;", &size, &outat); break;
                 case '\xf8' : append_output(&output, "&#x00B0;", &size, &outat); break;
                 case '\xf9' : append_output(&output, "&#x2219;", &size, &outat); break;
                 case '\xfa' : append_output(&output, "&#x00B7;", &size, &outat); break;
                 case '\xfb' : append_output(&output, "&#x221A;", &size, &outat); break;
                 case '\xfc' : append_output(&output, "&#x207F;", &size, &outat); break;
                 case '\xfd' : append_output(&output, "&#x00B2;", &size, &outat); break;
                 case '\xfe' : append_output(&output, "&#x25AA;", &size, &outat); break;
				case '<':	append_output(&output, "&lt;", &size, &outat); break;
				case '>':	append_output(&output, "&gt;", &size, &outat); break;
				case '\n':case 13: momline++;
									 line=0;
                                     append_output(&output, "<br />\n", &size, &outat);
                                     break;
                case ' ':	append_output(&output, "&nbsp;", &size, &outat); break;
				default:	{
                    sprintf(minibuf, "%c", c);
                    append_output(&output, minibuf, &size, &outat);
                }
			}
		}
	}

	//Footer
	if ((fc!=-1) || (bc!=-1) || (ul!=0) || (bo!=0) || (bl!=0))
        append_output(&output, "</span>\n", &size, &outat);
	
	return output;
}
