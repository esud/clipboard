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

#include <dirent.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
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

#define MD5_HASH_SIZE 32
#define MAX_NUMBER_OF_CLIPBOARD_ENTRIES 1000
#define MAX_CLIPBOARD_LABEL_SIZE 50
#define FONT "Sans 9"

//function prototypes
void retrieve_label_for_clipboard_entry(char *clipboardFilePath, char *clipboardLabel);
int file_path_with_modification_time_comparator(const void *a, const void *b);
time_t retrieve_file_modification_time(const char *path);
void on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer selection);
void add_to_list(GtkWidget *list, const gchar *string);

typedef struct
{
    time_t modificationTime;
    char filePath[255];
} FilePathWithModificationTime;

char clipboardEntryFilePathArray[MAX_NUMBER_OF_CLIPBOARD_ENTRIES][255];

//open /tmp/.clipboard_{username}/{md5_hash} file and load first 50 characters from this 
//file which we will use to display as label in clipboard data selection
void retrieve_label_for_clipboard_entry(char *clipboardFilePath, char *clipboardLabel)
{
    clipboardLabel[0] = '\0'; //reset data 
       
    //try to open clipboard file 
    FILE *fp = fopen(clipboardFilePath, "r");    
    if(fp == NULL) 
    {
        return;
    }
    
    int c; 
    int i = 0; 
    bool maxClipboardLabelSizeReached = false;
    while(true)
    {
        c = fgetc(fp);

        //if end of file is reached
        if(c == EOF || c == '\0')
        {
            break;
        }

        //replace new lines with space
        if(c == '\n')
        {
            c = ' ';
        }

        //if max clipboard label size is reached
        if(i > MAX_CLIPBOARD_LABEL_SIZE)
        {
            maxClipboardLabelSizeReached = true;
            break;
        }

        clipboardLabel[i] = c;
        i++;
    }
   
    //if there is a lot of content in this clipboard entry
    if(maxClipboardLabelSizeReached)
    {
        //we terminate the label with ...
        clipboardLabel[MAX_CLIPBOARD_LABEL_SIZE - 2] = '.'; 
        clipboardLabel[MAX_CLIPBOARD_LABEL_SIZE - 1] = '.'; 
        clipboardLabel[MAX_CLIPBOARD_LABEL_SIZE] = '.'; 
        clipboardLabel[MAX_CLIPBOARD_LABEL_SIZE + 1] = '\0'; 
    }
    else
    {
        clipboardLabel[i] = '\0'; 
    }
    
    //TODO: implement handling of utf-8 multibyte characters, like in cyrillic language
 
    fclose(fp);
}

int file_path_with_modification_time_comparator(const void *a, const void *b)
{
    FilePathWithModificationTime *filePathWithModificationTimeA = (FilePathWithModificationTime *)a;
    FilePathWithModificationTime *filePathWithModificationTimeB = (FilePathWithModificationTime *)b;
    return filePathWithModificationTimeB->modificationTime - filePathWithModificationTimeA->modificationTime;
}

time_t retrieve_file_modification_time(const char *path)
{
    struct stat statbuf;
    if(stat(path, &statbuf) == -1)
    {
        return 0;
    }
    return statbuf.st_mtime;
}

enum 
{
    LIST_ITEM = 0,
    N_COLUMNS
};

void on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer selection)
{
    GtkTreeIter iterator;
    GtkTreeModel *model;
    GtkTreePath *path;
    int *index;

    //if ESC is pressed
    if(event->keyval == GDK_Escape)
    {
        //output file path of last clipboard entry
        printf("%s", clipboardEntryFilePathArray[0]);

        //terminate application
        gtk_main_quit();
    }
    //if SPACE is pressed
    if(event->keyval == GDK_space)
    {
        //select next entry
        if(gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), &model, &iterator)) //try to set the iterator to the currently selected entry 
        {
            path = gtk_tree_model_get_path(model, &iterator);
            gtk_tree_path_next(path);
            gtk_tree_model_get_iter(model, &iterator, path);

            //HACK: we use this function instead of gtk_tree_selection_select_iter because
            //it is buggy, it moves the highlight but does not move the focus rectangle
            gtk_tree_view_set_cursor(gtk_tree_selection_get_tree_view(selection), path, NULL, FALSE);

            gtk_tree_view_scroll_to_cell(gtk_tree_selection_get_tree_view(selection), path, NULL, FALSE, 0, 0); 
        }
    }

    //if CTRL+V or ENTER is pressed
    if((event->state & GDK_CONTROL_MASK && event->keyval == GDK_v) || (event->state & GDK_CONTROL_MASK && event->keyval == GDK_V) || event->keyval == GDK_Return)
    {
        if(gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), &model, &iterator)) //try to set the iterator to the currently selected entry 
        {
            path = gtk_tree_model_get_path(model, &iterator);
            index = gtk_tree_path_get_indices(path);

            //output file path of selected clipboard entry
            printf("%s", clipboardEntryFilePathArray[index[0]]);
        }

        //terminate application
        gtk_main_quit();
    }
}

void add_to_list(GtkWidget *list, const gchar *string) 
{
    GtkListStore *store;
    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));

    GtkTreeIter iterator;
    gtk_list_store_append(store, &iterator);
    
    gtk_list_store_set(store, &iterator, LIST_ITEM, string, -1);
}

int main(int argc, char *argv[])
{
    //init gtk application
    gtk_init(&argc, &argv);

    //main window
    GtkWidget *window;
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Clipboard");
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE); //open window at the mouse position
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE); //do not display window bar

    //window size
    GdkGeometry hints;
    hints.min_width = 410;
    hints.max_width = 410;
    hints.min_height = 200;
    hints.max_height = 200;
    gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &hints, (GdkWindowHints)(GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE));

    //scrolled window
    GtkWidget *scrolledWindow;
    scrolledWindow = gtk_scrolled_window_new(NULL, NULL); 
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledWindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(window), scrolledWindow);

    //list view
    GtkWidget *list;
    list = gtk_tree_view_new();
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE); //hide the column header
    GtkCellRenderer *renderer;
    renderer = gtk_cell_renderer_text_new(); //renderer determines how the data will be displayed
    GtkTreeViewColumn *column;
    column = gtk_tree_view_column_new_with_attributes("dummy", renderer, "text", LIST_ITEM, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
    GtkListStore *store;
    store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
    g_object_unref(store);
    gtk_container_add(GTK_CONTAINER(scrolledWindow), list);

    //set font
    PangoFontDescription *pfd;  
    pfd = pango_font_description_from_string(FONT); 
    gtk_widget_modify_font(GTK_WIDGET(list), pfd);

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

    //retrieve list with clipboard files including modification time from /tmp/.clipboard_{username}/ folder 
    DIR *directoryStream;
    struct dirent *directoryEntry;
    time_t modificationTime;
    char filePath[255];
    FilePathWithModificationTime filePathWithModificationTimeArray[MAX_NUMBER_OF_CLIPBOARD_ENTRIES];
    int numberOfClipboardEntries = 0;
    directoryStream = opendir(storageFolder);
    if(!directoryStream)
    {
        fprintf(stderr, "Unable to open storage folder: %s\n", storageFolder);
        return EXIT_FAILURE;    
    }    
    while((directoryEntry = readdir(directoryStream)) != NULL)
    {
        //skip .. and . links
        if(strcmp(directoryEntry->d_name, "..") == 0 || strcmp(directoryEntry->d_name, ".") == 0)
        {
            continue;
        }
        
        //if not md5 hash
        if(strlen(directoryEntry->d_name) != MD5_HASH_SIZE)
        {
            continue;
        }
    
        //file path
        strcpy(filePath, storageFolder);
        strcat(filePath, directoryEntry->d_name);
        strcpy(filePathWithModificationTimeArray[numberOfClipboardEntries].filePath, filePath);
        
        //modification time
        modificationTime = retrieve_file_modification_time(filePath);
        filePathWithModificationTimeArray[numberOfClipboardEntries].modificationTime = modificationTime;
        
        numberOfClipboardEntries++;    
        if(numberOfClipboardEntries == MAX_NUMBER_OF_CLIPBOARD_ENTRIES)
        {
            break;
        }
    }
    closedir(directoryStream);

    //sort file names, recent files are listed first
    qsort(filePathWithModificationTimeArray, numberOfClipboardEntries, sizeof(FilePathWithModificationTime), file_path_with_modification_time_comparator);

    //create array with clipboard entry file path strings
    //and add label of each entry to the list
    char clipboardLabel[MAX_CLIPBOARD_LABEL_SIZE + 1];
    int i;
    for(i = 0; i < numberOfClipboardEntries; i++)
    {
        strcpy(clipboardEntryFilePathArray[i], filePathWithModificationTimeArray[i].filePath);
        retrieve_label_for_clipboard_entry(clipboardEntryFilePathArray[i], clipboardLabel);
        add_to_list(list, clipboardLabel);
    }
    
    //register quit application event handler
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);  

    //register key press event handler
    GtkTreeSelection *selection;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
    g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(on_key_press), selection);

    //show interface
    gtk_widget_show_all(window);
    gtk_main();

    return EXIT_SUCCESS;
}

