
#include <stdlib.h>
#include <windows.h>
#include <commdlg.h>

#include "Filter.h"
#include "ScriptInterpreter.h"
#include "ScriptValue.h"
#include "ScriptError.h"

#include "AviSynthDll.h"
#include "VirtualDubClip.h"
#include "resource.h"
#include "debug.h"


struct FilterData {
	long op, level, x, y, threshold, usechroma, width, height, opacity, color, color_temp;
	HBRUSH hbrColor;
	IFilterPreview* ifp;
};


extern "C" __declspec(dllexport) int __cdecl VirtualdubFilterModuleInit2(FilterModule* fm, const FilterFunctions* ff, int& vdfd_ver, int& vdfd_compat);
extern "C" __declspec(dllexport) void __cdecl VirtualdubFilterModuleDeinit(FilterModule* fm, const FilterFunctions* ff);

BOOL CALLBACK dlgWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

bool ChooseColor(HWND hWnd, COLORREF& color);
void UpdateEditText(HWND hDlg, int id, int msg, long* data, BOOL bSigned);
void UpdateScriptString(HWND hDlg, const FilterData* mfd);
void AvisynthScriptString(const FilterData* mfd, char* script);
void AvisynthScriptParam(const FilterData* mfd, char* script);

int InitProc(FilterActivation* fa, const FilterFunctions* ff);
void DeinitProc(FilterActivation* fa, const FilterFunctions* ff);
int RunProc(const FilterActivation* fa, const FilterFunctions* ff);
long ParamProc(FilterActivation* fa, const FilterFunctions* ff);
int ConfigProc(FilterActivation* fa, const FilterFunctions* ff, HWND hWnd);
void StringProc(const FilterActivation* fa, const FilterFunctions* ff, char* desc);
int StartProc(FilterActivation* fa, const FilterFunctions* ff);
int StopProc(FilterActivation* fa, const FilterFunctions* ff);

void ScriptConfigProc(IScriptInterpreter* isi, void* data, CScriptValue* argv, int argc);
bool ScriptStringProc(FilterActivation* fa, const FilterFunctions* ff, char* buf, int buflen);


ScriptFunctionDef scriptDef[] = {
	{ (ScriptFunctionPtr) ScriptConfigProc, "Config", "0iiiiiiiiii" },
	{ NULL },
};

CScriptObject script = {
	NULL, scriptDef, NULL
};

FilterDefinition filterDefIni = {
    0,0,NULL,
	"layertool",
    "AviSynth Layer support tool"
#ifdef _DEBUG
	" [DEBUG]"
#endif
	,
    "Unknown",
    NULL,
    sizeof(FilterData),
	InitProc,
    DeinitProc,
    RunProc,
    ParamProc,
    ConfigProc,
    StringProc,
    StartProc,
    StopProc,
	&script,
	ScriptStringProc,
};

enum LAYER_OP { OP_ADD=0, OP_SUBTRACT, OP_LIGHTEN, OP_DARKEN, OP_FAST, OP_MUL };
static char* op[] = { "add", "subtract", "lighten", "darken", "fast", "mul" };

FilterDefinition* filterDef = NULL;

bool preview = false;
char avisynth_sample[1000];
COLORREF custColors[16];

AviSynthDll* avisynth;
IScriptEnvironment* env;


BOOL CALLBACK dlgWndProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

	FilterData* mfd = (FilterData*) GetWindowLongPtr(hDlg, DWLP_USER);

    switch (message) {

		case WM_INITDIALOG:
			{
				mfd = (FilterData*) lParam;
				SetWindowLongPtr(hDlg, DWLP_USER, (LONG) mfd);

				mfd->color_temp = mfd->color;
				mfd->hbrColor = CreateSolidBrush(mfd->color);

				for (int i = 0; i < 6; i++) {
					SendDlgItemMessage(hDlg, IDC_FC_OPERATION, CB_ADDSTRING, 0, (LPARAM) op[i]);
				}

				SendDlgItemMessage(hDlg, IDC_FC_OPERATION, CB_SETCURSEL, mfd->op, 0);

				SendDlgItemMessage(hDlg, IDC_FC_LEVEL,		EM_LIMITTEXT, 3, 0);
				SendDlgItemMessage(hDlg, IDC_FC_POSX,		EM_LIMITTEXT, 8, 0);
				SendDlgItemMessage(hDlg, IDC_FC_POSY,		EM_LIMITTEXT, 8, 0);
				SendDlgItemMessage(hDlg, IDC_FC_THRESHOLD,	EM_LIMITTEXT, 3, 0);
				SendDlgItemMessage(hDlg, IDC_FC_WIDTH,		EM_LIMITTEXT, 8, 0);
				SendDlgItemMessage(hDlg, IDC_FC_HEIGHT,		EM_LIMITTEXT, 8, 0);
				SendDlgItemMessage(hDlg, IDC_FC_OPACITY,	EM_LIMITTEXT, 3, 0);

				SetDlgItemInt(hDlg, IDC_FC_LEVEL,		mfd->level,		FALSE);
				SetDlgItemInt(hDlg, IDC_FC_POSX,		mfd->x,			TRUE);
				SetDlgItemInt(hDlg, IDC_FC_POSY,		mfd->y,			TRUE);
				SetDlgItemInt(hDlg, IDC_FC_THRESHOLD,	mfd->threshold,	FALSE);
				SetDlgItemInt(hDlg, IDC_FC_WIDTH,		mfd->width,		FALSE);
				SetDlgItemInt(hDlg, IDC_FC_HEIGHT,		mfd->height,	FALSE);
				SetDlgItemInt(hDlg, IDC_FC_OPACITY,		mfd->opacity,	FALSE);

				CheckDlgButton(hDlg, IDC_FC_USECHROMA, mfd->usechroma);

				UpdateScriptString(hDlg, mfd);

				mfd->ifp->InitButton(GetDlgItem(hDlg, IDPREVIEW));
			}
            return (TRUE);

        case WM_COMMAND:
			{
				int id = LOWORD(wParam);
				int msg = HIWORD(wParam);

				switch (id) {

				case IDOK:
					{
						mfd = (FilterData*) GetWindowLongPtr(hDlg, DWLP_USER);
						mfd->color = mfd->color_temp;
						if (mfd->hbrColor) {
							DeleteObject(mfd->hbrColor);
							mfd->hbrColor = NULL;
						}
						EndDialog(hDlg, 0);
					}
					return TRUE;

				case IDCANCEL:
					{
						mfd = (FilterData*) GetWindowLongPtr(hDlg, DWLP_USER);
						if (mfd->hbrColor) {
							DeleteObject(mfd->hbrColor);
							mfd->hbrColor = NULL;
						}
						EndDialog(hDlg, 1);
					}
					return TRUE;

				case IDPREVIEW:
					mfd->ifp->Toggle(hDlg);
					break;

				case IDC_FC_OPERATION:
					if (msg == CBN_SELCHANGE) {

						mfd->op = SendDlgItemMessage(hDlg, IDC_FC_OPERATION, CB_GETCURSEL, 0, 0);

						UpdateScriptString(hDlg, mfd);
						mfd->ifp->RedoSystem();
					}
					break;

				case IDC_FC_LEVEL:		UpdateEditText(hDlg, id, msg, &mfd->level,		FALSE); break;
				case IDC_FC_POSX:		UpdateEditText(hDlg, id, msg, &mfd->x,			TRUE);  break;
				case IDC_FC_POSY:		UpdateEditText(hDlg, id, msg, &mfd->y,			TRUE);  break;
				case IDC_FC_THRESHOLD:	UpdateEditText(hDlg, id, msg, &mfd->threshold,	FALSE); break;
				case IDC_FC_WIDTH:		UpdateEditText(hDlg, id, msg, &mfd->width,		FALSE); break;
				case IDC_FC_HEIGHT:		UpdateEditText(hDlg, id, msg, &mfd->height,		FALSE); break;
				case IDC_FC_OPACITY:	UpdateEditText(hDlg, id, msg, &mfd->opacity,	FALSE); break;

				case IDC_FC_USECHROMA:
					if (msg == BN_CLICKED) {
						mfd->usechroma = IsDlgButtonChecked(hDlg, id);
					}
					break;

				case IDC_FC_PICK_COLOR:
					mfd = (FilterData*) GetWindowLongPtr(hDlg, DWLP_USER);

					if (ChooseColor(hDlg, (COLORREF&) mfd->color_temp)) {
						DeleteObject(mfd->hbrColor);
						mfd->hbrColor = CreateSolidBrush(mfd->color_temp);
						RedrawWindow(GetDlgItem(hDlg, IDC_FC_COLOR), NULL, NULL, RDW_ERASE|RDW_INVALIDATE|RDW_UPDATENOW);

						UpdateScriptString(hDlg, mfd);
						mfd->ifp->RedoSystem();
					}
					return TRUE;

				case IDC_FC_SCRIPT:
					if (msg == EN_SETFOCUS) {
						SendDlgItemMessage(hDlg, id, EM_SETSEL, 0, -1);
					}
					break;
				}
			}
            break;

		case WM_CTLCOLORSTATIC:
			if (GetWindowLong((HWND) lParam, GWL_ID) == IDC_FC_COLOR) {
				mfd = (FilterData*) GetWindowLongPtr(hDlg, DWLP_USER);
				return (BOOL) mfd->hbrColor;
			}
			break;
    }

	return FALSE;
}

void UpdateEditText(HWND hDlg, int id, int msg, long* data, BOOL bSigned) {
	FilterData* mfd = (FilterData*) GetWindowLongPtr(hDlg, DWLP_USER);
	long value;
	char cval[10];

	if (msg == EN_KILLFOCUS) {

		GetDlgItemText(hDlg, id, cval, 10);
		*data = value = strtoul(cval, NULL, 10);
		SetDlgItemInt(hDlg, id, value, bSigned);

		UpdateScriptString(hDlg, mfd);
		mfd->ifp->RedoSystem();
	}
}

void UpdateScriptString(HWND hDlg, const FilterData* mfd) {
	AvisynthScriptString(mfd, avisynth_sample);
	SetDlgItemText(hDlg, IDC_FC_SCRIPT, avisynth_sample);
}

bool ChooseColor(HWND hWnd, COLORREF& color) {
	CHOOSECOLOR cc;
	ZeroMemory(&cc, sizeof(CHOOSECOLOR));
	cc.lStructSize	= sizeof(CHOOSECOLOR);
	cc.hwndOwner	= hWnd;
	cc.lpCustColors	= (LPDWORD) custColors;
	cc.rgbResult	= color;
	cc.Flags		= CC_FULLOPEN | CC_RGBINIT;
	if (ChooseColor(&cc)) {
		color = cc.rgbResult;
		return true;
	}
	return false;
}

void AvisynthScriptString(const FilterData* mfd, char* avisynth_sample) {
	char param[1000];
	AvisynthScriptParam(mfd, param);
	sprintf(avisynth_sample, "Layer(base_clip, overlay_clip, %s)", param);
}

void AvisynthScriptParam(const FilterData* mfd, char* avisynth_sample) {
	sprintf(avisynth_sample, "op=\"%s\", level=%d, x=%d, y=%d, threshold=%d, use_chroma=%s",
		op[mfd->op], mfd->level, mfd->x, mfd->y, mfd->threshold, mfd->usechroma?"true":"false");
}

int InitProc(FilterActivation* fa, const FilterFunctions* ff) {
	FilterData* mfd = (FilterData*) fa->filter_data;
	ZeroMemory(mfd, sizeof(FilterData));
	mfd->op			= OP_ADD;
	mfd->level		= 255;
	mfd->opacity	= 255;
	mfd->usechroma	= TRUE;
	return 0;
}

void DeinitProc(FilterActivation* fa, const FilterFunctions* ff) {
}

int RunProc(const FilterActivation* fa, const FilterFunctions* ff) {

	FilterData* mfd = (FilterData*) fa->filter_data;

	if (!preview) return 0;
	if (!avisynth->ok) return 0;

	VBitmap* bitmap = &fa->src;
	PClip clip = new VirtualDubClip(bitmap);

	const char* name_blank[] = { "length", "width", "height", "pixel_type", "color" };
	const char* name_layer[] = { NULL, NULL, "op", "level", "x", "y", "threshold", "use_chroma" };

	int c = ((mfd->opacity & 0xff)<<24) | (((mfd->color_temp & 0xff)<<16) | (mfd->color_temp & 0xff00) | ((mfd->color_temp & 0xff0000)>>16));

	AVSValue args_blank[] = { AVSValue(1), AVSValue(mfd->width), AVSValue(mfd->height), AVSValue("RGB32"), AVSValue(c) };
	AVSValue args_layer[] = { AVSValue(), AVSValue(), AVSValue(op[mfd->op]), AVSValue(mfd->level), AVSValue(mfd->x), AVSValue(mfd->y), AVSValue(mfd->threshold), AVSValue((bool) mfd->usechroma) };

	try {
		PClip last = env->Invoke("ConvertToRGB32", AVSValue(clip)).AsClip();
		PClip blank = env->Invoke("BlankClip", AVSValue(args_blank, 5), name_blank).AsClip();

		args_layer[0] = last;
		args_layer[1] = blank;

		last = env->Invoke("Layer", AVSValue(args_layer, 8), name_layer).AsClip();

		PVideoFrame frame = last->GetFrame(fa->pfsi->lCurrentSourceFrame, env);

		env->BitBlt((BYTE*) bitmap->data, frame->GetPitch(), frame->GetReadPtr(), bitmap->pitch, bitmap->w * 4, bitmap->h);

	} catch (...) {
		DebugPrint("AviSynth error\n");
	}

	return 0;
}

long ParamProc(FilterActivation* fa, const FilterFunctions* ff) {
	fa->dst.offset = fa->src.offset;
	return 0;
}

int ConfigProc(FilterActivation* fa, const FilterFunctions* ff, HWND hWnd) {
	FilterData* mfd = (FilterData*) fa->filter_data;

	mfd->ifp = fa->ifp;

	preview = true;
	int ret = DialogBoxParam(fa->filter->module->hInstModule, MAKEINTRESOURCE(IDD_FILTER_CONFIG), hWnd, dlgWndProc, (LPARAM) mfd);
	preview = false;

	return ret;
}

void StringProc(const FilterActivation* fa, const FilterFunctions* ff, char* desc) {
    FilterData* mfd = (FilterData*) fa->filter_data;
	char param[1000];
	AvisynthScriptParam(mfd, param);
	sprintf(desc, " (%s)", param);
}

int StartProc(FilterActivation* fa, const FilterFunctions* ff) {
	env = NULL;
	if (!avisynth->ok) return 0;
	env = avisynth->CreateScriptEnvironment();
	return 0;
}

int StopProc(FilterActivation* fa, const FilterFunctions* ff) {
	if (!avisynth->ok) return 0;
	if (env) delete env;
	env = NULL;
	return 0;
}

void ScriptConfigProc(IScriptInterpreter* isi, void* data, CScriptValue* argv, int argc) {
	FilterActivation* fa = (FilterActivation*) data;
    FilterData* mfd = (FilterData*) fa->filter_data;
	mfd->op			= argv[0].asInt();
	mfd->level		= argv[1].asInt();
	mfd->x			= argv[2].asInt();
	mfd->y			= argv[3].asInt();
	mfd->threshold	= argv[4].asInt();
	mfd->usechroma	= argv[5].asInt();
	mfd->width		= argv[6].asInt();
	mfd->height		= argv[7].asInt();
	mfd->opacity	= argv[8].asInt();
	mfd->color		= argv[9].asInt();
}

bool ScriptStringProc(FilterActivation* fa, const FilterFunctions* ff, char* buf, int buflen) {
    FilterData* mfd = (FilterData*) fa->filter_data;
	_snprintf(buf, buflen, "Config(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)",
		mfd->op, mfd->level, mfd->x, mfd->y, mfd->threshold, mfd->usechroma, mfd->width, mfd->height, mfd->opacity, mfd->color);
	return true;
}

int __cdecl VirtualdubFilterModuleInit2(FilterModule* fm, const FilterFunctions* ff, int& vdfd_ver, int& vdfd_compat) {

	filterDef = ff->addFilter(fm, &filterDefIni, sizeof(FilterDefinition));
	if (filterDef == NULL)
		return 1;

	vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
	vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

	avisynth = new AviSynthDll("avisynth.dll");

	return 0;
}

void __cdecl VirtualdubFilterModuleDeinit(FilterModule* fm, const FilterFunctions* ff) {

	ff->removeFilter(filterDef);
	filterDef = NULL;

	delete avisynth;
}
