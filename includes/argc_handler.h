#ifndef ARG_HANDLER_H_DEFINED
#define ARG_HANDLER_H_DEFINED

#include <wx/wx.h>
#include <wx/app.h>
#include <wx/cmdline.h>



static const wxCmdLineEntryDesc cmdLineDesc[] =
{
    { wxCMD_LINE_SWITCH, "h", "help", "show this help message",
        wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
    { wxCMD_LINE_SWITCH, "f", "file", "open a folder",
        wxCMD_LINE_VAL_NONE, 0 },
    { wxCMD_LINE_SWITCH, "o", "open", "open file",
        wxCMD_LINE_VAL_NONE, wxCMD_LINE_HIDDEN },
    wxCMD_LINE_DESC_END
};

bool args_handler(int argc, char **argv)
{
   wxCmdLineParser parser(cmdLineDesc, argc, argv);
   auto value = parser.Parse();
   if(parser.Found("h"))
   {
      /// Show help.
   }
   return false;
}



#endif  /// ARG_HANDLER_H_DEFINED