#ifndef SPRITEBATCH_H
#define SPRITEBATCH_H

#include "Graphics/Vertex.h"

#include "GLEW/glew.h"
#include "Math/Maths.h"
#include "System/Types.h"

#include <vector>
#include <algorithm> 

#define GL_VERTEX_ATTRIB_POSITION    0
#define GL_VERTEX_ATTRIB_COLOR       1
#define GL_VERTEX_ATTRIB_UV          2

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

namespace Enjon { namespace Graphics {

	enum CoordinateFormat
	{
		CARTESIAN, 
		ISOMETRIC, 
		PROJECTION
	};

	enum class GlyphSortType 
	{ 
		NONE, 
		FRONT_TO_BACK, 
		BACK_TO_FRONT, 
		TEXTURE 
	};

	struct Glyph 
	{ 

		GLuint texture;
		float depth; 
		Vertex topLeft;
		Vertex bottomLeft;
		Vertex topRight;
		Vertex bottomRight;
	};

	inline Enjon::Math::Vec2 RotatePoint(const Enjon::Math::Vec2* Pos, float angle)
	{
		Enjon::Math::Vec2 NewVec;
		NewVec.x = Pos->x * cos(angle) - Pos->y * sin(angle);
		NewVec.y = Pos->x * sin(angle) + Pos->y * cos(angle);
		return NewVec;	
	}

	inline Glyph NewGlyph(const Enjon::Math::Vec4& destRect, const Enjon::Math::Vec4& uvRect, GLuint texture, float depth, const ColorRGBA16& color)
	{
		Glyph glyph;

		/* Set topLeft vertex */
		glyph.topLeft = NewVertex(destRect.x, destRect.y + destRect.w, uvRect.x, uvRect.y + uvRect.w, color.r, color.g, color.b, color.a);

		/* Set bottomLeft vertex */
		glyph.bottomLeft = NewVertex(destRect.x, destRect.y, uvRect.x, uvRect.y, color.r, color.g, color.b, color.a);

		/* Set bottomRight vertex */
		glyph.bottomRight = NewVertex(destRect.x + destRect.z, destRect.y, uvRect.x + uvRect.z, uvRect.y, color.r, color.g, color.b, color.a); 
		
		/* Set topRight vertex */
		glyph.topRight = NewVertex(destRect.x + destRect.z, destRect.y + destRect.w, uvRect.x + uvRect.z, uvRect.y + uvRect.w, color.r, color.g, color.b, color.a);

		/* Set texture */
		glyph.texture = texture;

		/* Set depth */
		glyph.depth = depth;

		return glyph;
	}

	inline Glyph NewGlyph(const Enjon::Math::Vec4& destRect, const Enjon::Math::Vec4& uvRect, GLuint texture, float depth, const ColorRGBA16& color, float angle, Graphics::CoordinateFormat Format)
	{
		Glyph glyph;

        // Enjon::Math::Vec2 halfDims(destRect.z / 2.0f, destRect.w / 2.0f);
        Enjon::Math::Vec2 halfDims(destRect.z / 2.0f, destRect.w / 2.0f);

        // Get points centered at origin
        Enjon::Math::Vec2 tl(-halfDims.x, halfDims.y);
        Enjon::Math::Vec2 bl(-halfDims.x, -halfDims.y);
        Enjon::Math::Vec2 br(halfDims.x, -halfDims.y);
        Enjon::Math::Vec2 tr(halfDims.x, halfDims.y);

        // auto r = 10.0f;
        // auto c = r * cos(angle);
        // auto s = r * sin(angle);
        // tl += Enjon::Math::Vec2(c, s);
        // tr += Enjon::Math::Vec2(c, s);
        // bl += Enjon::Math::Vec2(c, s);
        // br += Enjon::Math::Vec2(c, s);

        // Rotate the points back to left corner as pivot point
        // NOTE(John): Better way of doing this would be to rotate by a given point
        tl = RotatePoint(&tl, angle);
        bl = RotatePoint(&bl, angle);
        br = RotatePoint(&br, angle);
        tr = RotatePoint(&tr, angle);

        if (Format == CoordinateFormat::ISOMETRIC)
        {
	        tl = Enjon::Math::CartesianToIso(tl);
	        bl = Enjon::Math::CartesianToIso(bl);
	        br = Enjon::Math::CartesianToIso(br);
	        tr = Enjon::Math::CartesianToIso(tr);
        }


		/* Set topLeft vertex */
		glyph.topLeft = NewVertex(destRect.x + tl.x, destRect.y + tl.y, uvRect.x, uvRect.y + uvRect.w, color.r, color.g, color.b, color.a);

		/* Set bottomLeft vertex */
		glyph.bottomLeft = NewVertex(destRect.x + bl.x, destRect.y + bl.y, uvRect.x, uvRect.y, color.r, color.g, color.b, color.a);

		/* Set bottomRight vertex */
		glyph.bottomRight = NewVertex(destRect.x + br.x, destRect.y + br.y, uvRect.x + uvRect.z, uvRect.y, color.r, color.g, color.b, color.a); 
		
		/* Set topRight vertex */
		glyph.topRight = NewVertex(destRect.x + tr.x, destRect.y + tr.y, uvRect.x + uvRect.z, uvRect.y + uvRect.w, color.r, color.g, color.b, color.a);

		/* Set texture */
		glyph.texture = texture;

		/* Set depth */
		glyph.depth = depth;

		return glyph;
	}

	// TODO(John)::Consider making this strictly POD struct
	struct RenderBatch 
	{
		//RenderBatch(GLuint Offset, GLuint NumVertices, GLuint Texture) : offset(Offset),
		//	numVertices(NumVertices), texture(Texture) {
		//}

		GLuint offset;
		GLuint numVertices;
		GLuint texture;
	};

	inline RenderBatch NewRenderBatch(GLuint Offset, GLuint NumVerticies, GLuint Texture)
	{
		RenderBatch renderbatch;

		renderbatch.offset = Offset;
		renderbatch.numVertices = NumVerticies;
		renderbatch.texture = Texture;

		return renderbatch;
	}

	// TODO(John)::Might even consider making this strictly POD
	class SpriteBatch
	{
	public:
		SpriteBatch();
		~SpriteBatch();

		void Init();

		void Begin(GlyphSortType sortType = GlyphSortType::TEXTURE);
		void End();

		/* Adds glpyh to spritebatch to be rendered */
		void Add(const Enjon::Math::Vec4& destRect, const Enjon::Math::Vec4& uvRect, GLuint texture = 0, const ColorRGBA16& color = RGBA16(1.0f), float depth = 0.0f);

		/* Adds glpyh to spritebatch to be rendered with specified rotation */
		void Add(const Enjon::Math::Vec4& destRect, const Enjon::Math::Vec4& uvRect, GLuint texture, const ColorRGBA16& color, float depth, float angle, CoordinateFormat Format = CoordinateFormat::CARTESIAN);

		/* Renders entire batch to screen */
		void RenderBatch();

		/* Merges two sprite batches together */
		void MergeGlyphs(SpriteBatch* Other); 

		/* Gets the glyphs of this spritebatch */
		std::vector<Glyph>* GetGlyphs() { return &m_glyphs; }

		/* Gets the size of the render batch */
		inline unsigned int GetRenderBatchesSize() const { return m_renderBatches.size(); }



	private:
		void CreateRenderBatches();
		void CreateVertexArray();
		void SortGlyphs();

		static bool CompareFrontToBack(Glyph* a, Glyph* b);
		static bool CompareBackToFront(Glyph* a, Glyph* b);
		static bool CompareTexture(Glyph* a, Glyph* b);

		GLuint m_vbo;
		GLuint m_vao;

		GlyphSortType m_sortType;

		std::vector<Glyph> m_glyphs;
		std::vector<Glyph*> m_glyphpointers;
		std::vector<Enjon::Graphics::RenderBatch> m_renderBatches;
	};
}} 

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
#endif