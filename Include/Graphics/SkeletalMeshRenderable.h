// @file SkeletalMeshRenderable.h
// Copyright 2016-2018 John Jackson. All Rights Reserved.

#pragma once
#ifndef ENJON_SKELETAL_MESH_RENDERABLE_H
#define ENJON_SKELETAL_MESH_RENDERABLE_H

#include "System/Types.h"
#include "Graphics/Renderable.h"
#include "Graphics/SkeletalMesh.h"

namespace Enjon
{
	ENJON_CLASS( Construct )
	class SkeletalMeshRenderable : public Renderable
	{
		ENJON_CLASS_BODY( )

		public:

			/*
			* @brief
			*/
			SkeletalMeshRenderable( ) = default;

			/*
			* @brief
			*/
			~SkeletalMeshRenderable( ) = default;

			/**
			* @brief
			*/
			virtual void SetMesh( const Mesh* mesh ) override;

			/**
			* @brief
			*/
			virtual const Mesh* GetMesh() const override; 

			/**
			* @brief
			*/
			void SetMesh( const AssetHandle< SkeletalMesh >& mesh );

		protected:

			ENJON_PROPERTY( Editable, Delegates[ Mutator = SetMesh ] )
			AssetHandle< SkeletalMesh > mMesh;

		private:

	};
}

#endif
