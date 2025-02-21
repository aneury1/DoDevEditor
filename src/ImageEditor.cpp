#include "ImageEditor.h"

ImageEditor::ImageEditor(wxWindow* parent)
: EditorTab(parent)
{
   
}
    
int ImageEditor::getTypeOfEditor(){
    return 0;
}
    
std::vector<uint8_t> ImageEditor::getData(){
    return {};
}

Response ImageEditor::setData(std::vector<uint8_t> d){
     return Response::Success;
}

bool ImageEditor::wasChanged(){
    return false;
}

Response ImageEditor::saveDocument(){
    return Response::Success;
}

Response ImageEditor::openFile(std::string filepath){
     return Response::Success;
}