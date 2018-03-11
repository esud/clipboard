#!/bin/bash
/usr/local/bin/clipboard_selection | /usr/bin/xargs /usr/bin/cat | /usr/bin/xclip -r -selection clipboard
