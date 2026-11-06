/* Packaging launcher only; all application/UI/protocol logic is shared Python/C. */
#include <windows.h>
#include <wchar.h>
#include <stdlib.h>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR arguments,int show){
 (void)instance;(void)previous;(void)show;
 wchar_t folder[32768],python[32768],script[32768];
 DWORD size=GetModuleFileNameW(NULL,folder,32768);
 if(!size||size>=32768)return 1;
 wchar_t *slash=wcsrchr(folder,L'\\');if(!slash)return 1;*slash=0;
 if(wcslen(folder)+80>=32768)return 1;
 swprintf(python,32768,L"%ls\\runtime\\pythonw.exe",folder);
 swprintf(script,32768,L"%ls\\app\\launch.py",folder);
 size_t length=wcslen(python)+wcslen(script)+wcslen(arguments)+12;
 wchar_t *command=calloc(length,sizeof *command);if(!command)return 1;
 swprintf(command,length,L"\"%ls\" \"%ls\" %ls",python,script,arguments);
 STARTUPINFOW startup={0};PROCESS_INFORMATION process={0};startup.cb=sizeof startup;
 BOOL ok=CreateProcessW(python,command,NULL,NULL,FALSE,0,NULL,folder,&startup,&process);free(command);
 if(!ok){MessageBoxW(NULL,L"Unable to start the bundled runtime. Extract the complete ZIP before launching the EXE.",L"rusEFI Wideband Manager 0.6",MB_ICONERROR|MB_OK);return 1;}
 CloseHandle(process.hThread);WaitForSingleObject(process.hProcess,INFINITE);DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);return (int)code;
}
