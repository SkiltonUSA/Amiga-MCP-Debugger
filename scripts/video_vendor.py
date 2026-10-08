"""Generate the narrowly patched PL_MPEG header; keep upstream bytes intact."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def prepare(out):
    vendor=ROOT/'amiga/video/vendor'
    pin=json.loads((vendor/'upstream.json').read_text())
    raw=(vendor/'pl_mpeg.h').read_bytes()
    if hashlib.sha256(raw).hexdigest()!=pin['sha256']:raise ValueError('PL_MPEG pin mismatch')
    text=raw.decode()
    # Rational frame scheduling lives on 68k. Avoid soft-double runtime solely
    # for unused presentation timestamps; IDCT/motion/pixel conversion unchanged.
    patches={
        "self->picture_type == PLM_VIDEO_PICTURE_TYPE_PREDICTIVE\n\t\t\t\t\t)":
            "self->picture_type == PLM_VIDEO_PICTURE_TYPE_PREDICTIVE ||\n\t\t\t\t\t\tself->picture_type == PLM_VIDEO_PICTURE_TYPE_B\n\t\t\t\t\t)",
        "void plm_buffer_discard_read_bytes(plm_buffer_t *self) {":
            "void plm_buffer_discard_read_bytes(plm_buffer_t *self) {\n\tif (self->mode == PLM_BUFFER_MODE_FIXED_MEM) return; /* immutable replayable video input */",
        "double pixel_aspect_ratio;": "float pixel_aspect_ratio; /* retain upstream float table without a runtime conversion */",
        'self->time = (double)self->frames_decoded / self->framerate;':
            '/* ZZVideo: timestamps unused; 68k schedules using the sequence rational. */',
        'plm_buffer_t *self = (plm_buffer_t *)PLM_MALLOC(sizeof(plm_buffer_t));\n\tmemset':
            'plm_buffer_t *self = (plm_buffer_t *)PLM_MALLOC(sizeof(plm_buffer_t));\n\tif (!self) return NULL;\n\tmemset',
        'plm_video_t *self = (plm_video_t *)PLM_MALLOC(sizeof(plm_video_t));\n\tmemset':
            'plm_video_t *self = (plm_video_t *)PLM_MALLOC(sizeof(plm_video_t));\n\tif (!self) return NULL;\n\tmemset',
        'self->frames_data = (uint8_t*)PLM_MALLOC(frame_data_size * 3);':
            'self->frames_data = (uint8_t*)PLM_MALLOC(frame_data_size * 3);\n\tif (!self->frames_data) return FALSE;',
    }
    for old,new in patches.items():
        if old not in text:raise ValueError('PL_MPEG patch no longer applies')
        text=text.replace(old,new)
    out.mkdir(parents=True,exist_ok=True)
    (out/'pl_mpeg_port.h').write_text(text)
    return pin
