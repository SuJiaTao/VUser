
/* ========== <vpanelshader.c>					==========	*/
/* Bailey Jia-Tao Brown							2022		*/
/* Internal panel shader logic								*/

/* ========== INCLUDES							==========	*/
#define GLEW_STATIC
#include "glew.h"
#include "vpanelshader.h"
#include <stdio.h>
#include <math.h>


/* ========== SHADER SOURCE						==========	*/
static vPCHAR panelVertSrc =
"#version 460 core\n"
"\n"
"layout (location = 0) in vec2 v_position;\n"
"layout (location = 1) uniform vec4 v_color;\n"
"layout (location = 2) uniform mat4 v_projMatrix;\n"
"layout (location = 3) uniform mat4 v_modelMatrix;\n"
"layout (location = 4) uniform mat4 v_textureMatrix;\n"
"\n"
"out vec4 f_color;\n"
"out vec2 f_textureUV;\n"
"\n"
"void main()\n"
"{\n"
"\tf_color\t\t= v_color;\n"
"\tvec4 tex4 = v_textureMatrix * vec4(v_position, 0.0, 1.0);\n"
"\tf_textureUV = tex4.xy;\n"
"\tgl_Position = v_projMatrix * v_modelMatrix * vec4(v_position, 0.0, 1.0);\t\t\n"
"}\n"
"";

static vPCHAR panelFragSrc =
"#version 460 core\n"
"\n"
"in vec2 f_textureUV;\n"
"in vec4 f_color;\n"
"\n"
"uniform sampler2D f_texture;\n"
"out vec4 FragColor;"
"\n"
"void main()\n"
"{\n"
"\tFragColor = texture(f_texture, f_textureUV) * f_color;\n"
"\t/* dithering alogrithm */\n"
"\tif (FragColor.a <= 0.97)\n"
"\t{\n"
"\t\tif (FragColor.a <= 0.03) discard;\n"
"\t\tint x = int(gl_FragCoord.x);\n"
"\t\tint y = int(gl_FragCoord.y);\n"
"\n"
"\t\tif (FragColor.a < 0.5)\n"
"\t\t{\n"
"\t\t\tint discardInterval = int(1.0f / FragColor.a);\n"
"\t\t\tint step = x + (y * (discardInterval >> 1)) + (y * (discardInterval >> 3));\n"
"\t\t\tif (step % discardInterval != 0) discard;\n"
"\t\t}\n"
"\t\telse\n"
"\t\t{\n"
"\t\t\tint discardInterval = int(1.0f / (1.0f - FragColor.a));\n"
"\t\t\tint step = x + (y * (discardInterval >> 1)) + (y * (discardInterval >> 3));\n"
"\t\t\tif (step % discardInterval == 0) discard;\n"
"\t\t}\n"
"\t}\n"
"\n"
"\tFragColor.a = 1.0;\n"
"}\n"
"";


/* ========== HELPER FUNCTIONS					==========	*/
static void UPanelUpdateIterateFunc(vHNDL buffer, vUI16 index, vPUPanel panel, 
	vPTR input)
{
	/* handle mouse over state */
	vBOOL lastMouseOverState = panel->mouseOver;
	panel->mouseOver = vUIsMouseOverPanel(panel);

	if (panel->mouseOver != lastMouseOverState &&
		panel->style->mouseBhv.onMouseOverFunc != NULL)
	{
		if (panel->mouseOver == TRUE)
		{
			panel->style->mouseBhv.onMouseOverFunc(panel);
		}
		else
		{
			panel->style->mouseBhv.onMouseAwayFunc(panel);
		}
	}
		
	/* handle click function */
	vBOOL lastMouseClickState = panel->mouseClick;
	panel->mouseClick = (GetKeyState(VK_LBUTTON) & 0x8000) && panel->mouseOver;
	if (panel->mouseClick != lastMouseClickState &&
		panel->style->mouseBhv.onMouseClickFunc != NULL)
	{
		if (panel->mouseClick == TRUE)
		{
			panel->style->mouseBhv.onMouseClickFunc(panel);
		}
		else
		{
			panel->style->mouseBhv.onMouseUnclickFunc(panel);
		}
	}
	
	/* call continuous functions */
	if (panel->mouseClick == TRUE &&
		panel->style->mouseBhv.mouseClickFunc != NULL)
		panel->style->mouseBhv.mouseClickFunc(panel);

	if (panel->mouseOver == TRUE &&
		panel->style->mouseBhv.mouseOverFunc != NULL)
		panel->style->mouseBhv.mouseOverFunc(panel);
}

static void UPanelDrawRect(vPUPanel panel, vGColor color, vGRect rectOverride)
{
	/* get old projection matrix */
	float oldProjMatrix[16];
	glGetFloatv(GL_PROJECTION_MATRIX, oldProjMatrix);

	/* translate projection */
	glMatrixMode(GL_PROJECTION);
	glTranslatef(rectOverride.left, rectOverride.bottom, 0.0f);
	float panelWidth = rectOverride.right - rectOverride.left;
	float panelHeight = rectOverride.top - rectOverride.bottom;
	glScalef(panelWidth, panelHeight, 1.0f);

	/* setup texture matrix */
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	float textureSkinZoomScale = 1.0f / (float)(panel->skin->skinCount + 1);
	glTranslatef(panel->renderSkin * textureSkinZoomScale, 0.0f, 0.0f);
	glScalef(textureSkinZoomScale, 1.0f, 1.0f);

	/* clear model matrix */
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, GUI_DEPTH);

	/* setup render settings */
	glActiveTexture(GL_TEXTURE0);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	/* default to use completely white texture */
	glBindTexture(GL_TEXTURE_2D, _vuser.panelNoSkinTexture);

	/* if image exists for panel, use that */
	if (panel->skin != NULL)
		glBindTexture(GL_TEXTURE_2D, panel->skin->glHandle);

	/* bind to buffer and vertex array */
	glBindVertexArray(_vuser.panelShaderVertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, _vuser.panelShaderMesh);

	/* retrieve all data from gl matrix stack */
	GLfloat projectionMatrix[0x10];
	GLfloat modelMatrix[0x10];
	GLfloat textureMatrix[0x10];
	glGetFloatv(GL_PROJECTION_MATRIX, projectionMatrix);
	glGetFloatv(GL_MODELVIEW_MATRIX, modelMatrix);
	glGetFloatv(GL_TEXTURE_MATRIX, textureMatrix);


	/* apply uniform values */
	glUniform4fv(1, 1, &color);
	glUniformMatrix4fv(2, 1, GL_FALSE, projectionMatrix);
	glUniformMatrix4fv(3, 1, GL_FALSE, modelMatrix);
	glUniformMatrix4fv(4, 1, GL_FALSE, textureMatrix);

	/* draw outer box */
	glDrawArrays(GL_QUADS, 0, 4);

	/* load old projection matrix */
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(oldProjMatrix);
}

static void UPanelShaderRenderIterateFunc(vHNDL hndl, vUI16 index,
	vPUPanel panel, vPTR input)
{
	vGRect drawRect;
	vGRect innerRect;

	/* switch render method based on panel type */
	switch (panel->panelType)
	{
	/* most basic case */
	case vUPanelType_Rect:

		/* calculate inner rect */
		innerRect = vUCreateRectExpanded(panel->boundingBox,
			-panel->style->borderWidth);

		/* draw inner rect */
		UPanelDrawRect(panel, panel->style->fillColor, innerRect);

		/* draw outer rect */
		UPanelDrawRect(panel, panel->style->borderColor, panel->boundingBox);

		break;

	case vUPanelType_Button:
		
		drawRect = panel->boundingBox;

		/* calculate expanded/shrunk rect based on state */
		if (panel->mouseOver == TRUE)
		{
			drawRect = vUCreateRectExpanded(drawRect,
				panel->style->buttonHoverWidth);
		}
		if (panel->mouseClick == TRUE)
		{
			drawRect = vUCreateRectExpanded(drawRect,
				panel->style->buttonClickWidth);
		}

		/* calculate inner rect */
		innerRect = vUCreateRectExpanded(drawRect,
			-panel->style->borderWidth);

		/* draw inner rect */
		UPanelDrawRect(panel, panel->style->fillColor, innerRect);

		/* draw outer rect */
		UPanelDrawRect(panel, panel->style->borderColor, drawRect);
		
		break;

	default:
		break;
	}
}


/* ========== SHADER FUNCTIONS					==========	*/
void vUPanel_shaderInitFunc(vPGShader shader, vPTR shaderData, vPTR input)
{
	/* init glew */
	glewInit();

	/* gen array and buffer */
	glGenVertexArrays(1, &_vuser.panelShaderVertexArray);
	glBindVertexArray(_vuser.panelShaderVertexArray);

	glGenBuffers(1, &_vuser.panelShaderMesh);
	glBindBuffer(GL_ARRAY_BUFFER, _vuser.panelShaderMesh);
	float baseRect[4][2] = { 
		{ 0.0f, 0.0f }, 
		{ 0.0f, 1.0f }, 
		{ 1.0f, 1.0f },
		{ 1.0f, 0.0f } 
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(baseRect), baseRect, GL_STATIC_DRAW);

	/* init vertex array */
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), NULL);
	glEnableVertexAttribArray(0);

	/* init noskin texture */
	glGenTextures(1, &_vuser.panelNoSkinTexture);
	glBindTexture(GL_TEXTURE_2D, _vuser.panelNoSkinTexture);
	vBYTE texData[4] = { 255, 255, 255, 255 };
	glTexImage2D(GL_TEXTURE_2D, ZERO, GL_RGBA, 1, 1, ZERO, GL_RGBA,
		GL_UNSIGNED_BYTE, texData);

	/* forced linear filter */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	/* no wrap texture */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
}

void vUPanel_shaderRenderFunc(vPGShader shader, vPTR shaderdata, vPObject object,
	vPGRenderable renderable)
{
	/* update all panel objects */
	vBufferIterate(_vuser.panelList, UPanelUpdateIterateFunc, NULL);

	/* draw all panel objects */
	vBufferIterate(_vuser.panelList, UPanelShaderRenderIterateFunc, NULL);
}

vPCHAR UGetPanelShaderVertexSource(void)
{
	return panelVertSrc;
}

vPCHAR UGetPanelShaderFragmentSource(void)
{
	return panelFragSrc;
}