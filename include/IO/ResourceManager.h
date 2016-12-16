#ifndef ENJON_RESOURCEMANAGER_H
#define ENJON_RESOURCEMANAGER_H

#include <string>

#include "IO/TextureCache.h"
#include "IO/ObjLoader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Enjon { namespace Input { namespace ResourceManager { 

		Enjon::Graphics::GLTexture GetTexture(const std::string& texturePath, GLint magParam = GL_LINEAR, GLint minParam = GL_LINEAR_MIPMAP_LINEAR);
		Enjon::Graphics::MeshInstance* GetMesh(const char* MeshPath);

}}}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif