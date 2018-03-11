// +---------------------------------------------------------------------------+
// | Clipboard                                                                 |
// | Copyright 2018 Eduard Sudnik (http://esud.info/)                          |
// |                                                                           |
// | Permission is hereby granted, free of charge, to any person obtaining a   |
// | copy of this software and associated documentation files (the "Software"),|
// | to deal in the Software without restriction, including without limitation |
// | the rights to use, copy, modify, merge, publish, distribute, sublicense,  |
// | and/or sell copies of the Software, and to permit persons to whom the     |
// | Software is furnished to do so, subject to the following conditions:      |
// |                                                                           |
// | The above copyright notice and this permission notice shall be included   |
// | in all copies or substantial portions of the Software.                    |
// |                                                                           |
// | THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS   |
// | OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF                |
// | MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN |
// | NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,  |
// | DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR     |
// | OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE |
// | USE OR OTHER DEALINGS IN THE SOFTWARE.                                    |
// +---------------------------------------------------------------------------+

#include <ctype.h>
#include <openssl/md5.h> 
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h> 
#include <time.h>
#include <unistd.h> 
#include <utime.h>

#define RETRIEVE_CLIPBOARD_DATA_COMMAND "DISPLAY=:0 /usr/bin/xclip -o -selection clipboard 2>/dev/null"
#define MD5_HASH_SIZE 32

//clipboard data will be ignored  
//when it not matches this limits
#define MAX_CLIPBOARD_DATA_SIZE 1048576 //1 MiB
#define MAX_CLIPBOARD_LINE_SIZE 10240 //10 KiB

//function prototypes
bool file_exists(char *filePath);
bool retrieve_data_from_clipboard(char *data);
void create_md5_hash(char *string, char *md5Hash);
void write_to_file(char *filePath, char *data);
void trim_string(char *string);

//check if file exists
bool file_exists(char *filePath)
{
    struct stat buffer;
    return stat(filePath, &buffer) == 0;
}

bool retrieve_data_from_clipboard(char *data)
{
    //reset data 
    data[0] = '\0'; 
    
    //execute shell command to retrieve current clipboard data
    FILE *fp = popen(RETRIEVE_CLIPBOARD_DATA_COMMAND, "r");    
    if(fp == NULL) 
    {   
        //error
        return false;
    }

    //for each line
    char line[MAX_CLIPBOARD_LINE_SIZE + 2]; //+1 because of zero terminator and +1 because of check if max size limit is reached
    while(fgets(line, MAX_CLIPBOARD_LINE_SIZE + 2, fp) != NULL)
    {
        //if max clipboard line size is reached
        if(strlen(line) > MAX_CLIPBOARD_LINE_SIZE)
        {
            //error
            pclose(fp);   
            data[0] = '\0'; //reset data 
            return false;
        }

        //if max clipboard data size is reached
        if(strlen(data) + strlen(line) > MAX_CLIPBOARD_DATA_SIZE)
        {
            //error
            pclose(fp);    
            data[0] = '\0'; //reset data 
            return false;
        }

        strcat(data, line);
    }
        
    //make sure that all new lines and spaces are removed
    trim_string(data);

    pclose(fp);    
    return true;    
}

void create_md5_hash(char *string, char *md5Hash)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    int i;

    MD5(string, strlen(string), digest);

    //make sure that md5 hash is empty
    md5Hash[0] = '\0';

    for(i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        sprintf(md5Hash + strlen(md5Hash), "%02x", digest[i]);
    }
}

void write_to_file(char *filePath, char *data)
{
    FILE *fp = fopen(filePath, "w+");
    if(fp == NULL) 
    {
        return;
    }

    fputs(data, fp);
    fclose(fp);
}

void trim_string(char *string)
{
    int i;
    int startPosition = 0;
    int endPosition = strlen(string) - 1;

    //determine number of characters which need to remove from the beginning of the string
    while(isspace((unsigned char)string[startPosition]))
    {
        startPosition++;
    }

    //if the whole string contains 'spaces'
    //or string is empty
    if(startPosition == strlen(string))
    {
        //terminate with empty string
        string[0] = '\0';
        return;
    }

    //determine number of characters which need to remove from the end of the string
    while(isspace((unsigned char)string[endPosition]))
    {
        endPosition--;
    }

    //shift all characters to the start of the string
    for(i = startPosition; i <= endPosition; i++)
    { 
        string[i - startPosition] = string[i];
    }

    //add null string terminator
    string[i - startPosition] = '\0';
}

int main(int argc, char *argv[]) 
{
    char md5Hash[MD5_HASH_SIZE + 1]; //+1 because of zero terminator
    char previousMd5Hash[MD5_HASH_SIZE + 1]; //+1 because of zero terminator
    char clipboardData[MAX_CLIPBOARD_DATA_SIZE + 1]; //+1 because of zero terminator
    char clipboardFilePath[255]; 
    
    //set empty string
    md5Hash[0] = '\0';
    previousMd5Hash[0] = '\0';
    clipboardData[0] = '\0';
    clipboardFilePath[0] = '\0';

    //build storage folder path
    char storageFolder[255];
    strcpy(storageFolder, "/tmp/.clipboard_");
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);
    if(!pw)
    {
        fprintf(stderr, "Unable to build storage folder\n");
        return EXIT_FAILURE;
    } 
    strcat(storageFolder, pw->pw_name);
    strcat(storageFolder, "/");

    //make sure that storage folder exists
    mkdir(storageFolder, 0700);
    
    //do forever
    while(true)
    {
        //wait 0,5 sec
        usleep(1000000 / 2);

        //retrieve clipboard data
        if(!retrieve_data_from_clipboard(clipboardData))
        {
            continue;
        }     

        //if clipboard is empty
        if(clipboardData[0] == '\0')
        { 
            continue;
        }

        //create md5 hash of current clipboard data
        create_md5_hash(clipboardData, md5Hash);

        //if current clipboard data is the same like previous clipboard data
        if(strcmp(md5Hash, previousMd5Hash) == 0)
        {
            continue;
        }

        //update previousMd5Hash
        strcpy(previousMd5Hash, md5Hash);

        //clipboard file path
        strcpy(clipboardFilePath, storageFolder);
        strcat(clipboardFilePath, md5Hash);

        //if clipboard file already exists
        if(file_exists(clipboardFilePath))
        {
            //update file modification time
            utime(clipboardFilePath, NULL);
        }
        //if clipboard file not yet exists
        else
        {
            //make sure that storage folder exists
            mkdir(storageFolder, 0700);

            //create new file
            write_to_file(clipboardFilePath, clipboardData);
        }
    }
    
    return EXIT_SUCCESS;
}


