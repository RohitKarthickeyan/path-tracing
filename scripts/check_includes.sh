#!/bin/sh
# Fails if a src/ file includes a header its layer forbids (src/CONTEXT.md).
cd "$(dirname "$0")/.." || exit 1
fail=0
check() {
  re=$1; shift
  for f in "$@"; do
    [ -f "$f" ] || continue
    if grep -nE "#include *[<\"]($re)" "$f"; then echo "  ^ forbidden include in $f"; fail=1; fi
  done
}
GL='glad|GLFW|imgui|cuda_gl_interop'
check "cuda|$GL|stb_" src/hd.h src/camera.h src/scene.h src/intersect.h src/sampling.h
check "cuda|$GL" src/image_io.* src/obj.* src/bvh.* src/scenes.*
check "$GL" src/render.* src/filters.*
check "stb_" $(ls src/* 2>/dev/null | grep -v 'image_io\.cpp$')
exit $fail
