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
    int size;
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
                                              append_output(&output, "color:dimgray;", &size, &outat);
											 break; //Black
							case	1: 
                                             append_output(&output, "color:red;", &size, &outat);
											 break; //Red
							case	2: 
                                             append_output(&output, "color:lime;", &size, &outat);
											 break; //Green
							case	3: 
                                             append_output(&output, "color:yellow;", &size, &outat);
											 break; //Yellow
							case	4: 
                                             append_output(&output, "color:#3333FF;", &size, &outat);
											 break; //Blue
							case	5: 

                                             append_output(&output, "color:fuchsia;", &size, &outat);
											 break; //Purple
							case	6: 
                                             append_output(&output, "color:aqua;", &size, &outat);
											 break; //Cyan
							case	7: 
                                             append_output(&output, "color:white;", &size, &outat);
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
						if (bo)
						{
                            append_output(&output, "font-weight:bold;", &size, &outat);
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

                append_output(&output, " ", &size, &outat);
			line=0;
			momline++;
			append_output(&output, "\n", &size, &outat);
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
				case '&':	append_output(&output, "&amp;", &size, &outat); break;
				case '\"': append_output(&output, "&quot;", &size, &outat); break;
				case '<':	append_output(&output, "&lt;", &size, &outat); break;
				case '>':	append_output(&output, "&gt;", &size, &outat); break;
				case '\n':case 13: momline++;
									 line=0;
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
