#ifndef VIRTUALDUBCLIP_H
#define VIRTUALDUBCLIP_H

#include "avisynth.h"
#include "VBitmap.h"

class VirtualDubClip : public IClip {

private:
	VideoInfo _vi;
	PVideoFrame _frame;
	const VBitmap* _bitmap;

public:
	VirtualDubClip(const VBitmap* bitmap) {
		_frame = NULL;
		_bitmap = bitmap;
		GetVideoInfo(_vi, bitmap);
	}

	void __stdcall SetCacheHints(int cachehints, int frame_range) {}
	void __stdcall GetAudio(void* buf, __int64 start, __int64 count, IScriptEnvironment* env) {}
	bool __stdcall GetParity(int n)	{ return false; }
	const VideoInfo& __stdcall GetVideoInfo() { return _vi; }

	// from warpsharp
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) {
		if (!_frame) {
			_frame = env->NewVideoFrame(_vi);
			env->BitBlt(_frame->GetWritePtr(), _frame->GetPitch(), (BYTE*) _bitmap->data, _bitmap->pitch, _bitmap->w * 4, _bitmap->h);
		}
		return _frame;
	}

	void GetVideoInfo(VideoInfo& vi, const VBitmap* bitmap) {
		ZeroMemory(&vi, sizeof(VideoInfo));
		vi.width			= bitmap->w;
		vi.height			= bitmap->h;
		vi.fps_numerator	= 2997;
		vi.fps_denominator	= 100;
		vi.num_frames		= 1;
		vi.pixel_type		= VideoInfo::CS_BGR32;
	}
};

#endif
