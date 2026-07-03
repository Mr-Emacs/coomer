#ifndef BM_SHADERS_H
#define BM_SHADERS_H

static const char *BM_VERT_SHADER_SRC =
    "#version 130\n"
    "in vec2 aPos;\n"
    "in vec2 aTexCoord;\n"
    "out vec2 texcoord;\n"
    "\n"
    "uniform vec2 cameraPos;\n"
    "uniform float cameraScale;\n"
    "uniform vec2 windowSize;\n"
    "uniform vec2 screenshotSize;\n"
    "uniform vec2 cursorPos;\n"
    "\n"
    "vec2 to_world(vec2 v) {\n"
    "    vec2 ratio = vec2(\n"
    "        windowSize.x / screenshotSize.x / cameraScale,\n"
    "        windowSize.y / screenshotSize.y / cameraScale);\n"
    "    return vec2((v.x / screenshotSize.x * 2.0 - 1.0) / ratio.x,\n"
    "                (v.y / screenshotSize.y * 2.0 - 1.0) / ratio.y);\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "	gl_Position = vec4(to_world((aPos.xy - cameraPos * vec2(1.0, -1.0))), 0.0, 1.0);\n"
    "	texcoord = aTexCoord;\n"
    "}\n"
    ;

static const char *BM_FRAG_SHADER_SRC =
    "#version 130\n"
    "out mediump vec4 color;\n"
    "in mediump vec2 texcoord;\n"
    "uniform sampler2D tex;\n"
    "uniform vec2 cursorPos;\n"
    "uniform vec2 windowSize;\n"
    "uniform float flShadow;\n"
    "uniform float flRadius;\n"
    "uniform float cameraScale;\n"
    "uniform bool mirror;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 cursor = vec4(cursorPos.x, windowSize.y - cursorPos.y, 0.0, 1.0);\n"
    "    vec2 effective_texcoord = texcoord;\n"
    "    if (mirror) {\n"
    "        effective_texcoord.x = 1 - effective_texcoord.x;\n"
    "    }\n"
    "    color = mix(\n"
    "        texture(tex, effective_texcoord), vec4(0.0, 0.0, 0.0, 0.0),\n"
    "        length(cursor - gl_FragCoord) < (flRadius * cameraScale) ? 0.0 : flShadow);\n"
    "}\n"
    ;

#endif // BM_SHADERS_H

