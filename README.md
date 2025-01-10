# DoDevEditor
simply another  text editor previously was a try in Java, it contained nothing
now I want to try in c++ which is a powerfull languange, I will use for my own 
day to day task and is not intended to run instead of vscode, vim or other editor
I did just only for my own usage, I/m just want to share. 

# License. 

https://www.gnu.org/licenses/lgpl-3.0.html#license-text

# what is the DoD ?

well and editor with basic feature. with some kind of plugin system. at least I expect
these:

- Edit Code
- Open folder and show its structure.
- Syntax Highlight(at least for C++)
- Menu for Editing text in a simple way.
- For Search in open and other files in a folder structure

# Plugin System 

Well I Would like to allow plugin's developers, to add other menus in main menu. 
the layout would be similar to other editors side panel for project navigation and other stuff,
bottom container, toolbar with editor inside(this could be customizable to add differents of kind of editor), 
probably more than that but let leave as is now.

# Dependecies

by now I want to use C++ and wxwidgets, probably for settings json or xml files and then plugins should be in c++, for testing right google test.

# planning in the near future but not sure yet

use libgit2, libclang(tooling c++ support or similar/)
