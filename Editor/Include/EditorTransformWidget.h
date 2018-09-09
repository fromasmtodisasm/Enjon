// @file EditorTransformWidget.h
// Copyright 2016-2017 John Jackson. All Rights Reserved.

#pragma once
#ifndef ENJON_EDITOR_TRANSFORM_WIDGET_H
#define ENJON_EDITOR_TRANSFORM_WIDGET_H
 
#include "EditorWidget.h"
#include "EditorTranslationWidget.h"
#include "EditorScaleWidget.h"
#include "EditorRotationWidget.h"

#include <Graphics/Renderable.h>

namespace Enjon
{ 
	class EditorApp;

	enum class TransformationMode
	{
		Translation,
		Scale,
		Rotation
	};

	class EditorTransformWidget
	{ 
		public:
			EditorTransformWidget( ) = default;
			~EditorTransformWidget( ) = default;

			void Initialize( EditorApp* editor );
			void Update( );

			Transform GetWorldTransform( );

			void SetPosition( const Vec3& position );
			void SetScale( const f32& scale );
			void SetRotation( const Quaternion& rotation );
			void SetRotation( const Vec3& eulerAngles );

			EditorTranslationWidget mTranslationWidget;
			EditorScaleWidget mScaleWidget;
			EditorRotationWidget mRotationWidget;

			void BeginInteraction( TransformWidgetRenderableType type, const Transform& transform );
			void InteractWithWidget( Transform* transform );
			void EndInteraction( );
			TransformWidgetRenderableType GetInteractedWidgetType( );
			TransformationMode GetTransformationMode( ); 
			TransformSpace GetTransformSpace( ) const;

			void SetTransformSpace( TransformSpace space );

			bool IsInteractingWithWidget( ) const;
			Vec3 GetDelta( ) const; 
			f32 GetAngleDelta( ) const;
			Quaternion GetDeltaRotation( ) const;

			void SetTransformationMode( TransformationMode mode ); 

			void Enable( bool enable );

			static bool IsValidID( const u32& id );

			Vec3 GetTranslationSnap( ) const;
			void SetTranslationSnap( const Vec3& snap );

			Vec3 GetScaleSnap( ) const;
			void SetScaleSnap( const Vec3& snap );

			Vec3 GetIntersectionStartPosition( ) const;
			Vec3 GetAccumulatedTranslationDelta( ) const;
			Vec3 GetRootStartPosition( ) const;
			Vec3 GetRootStartScale( ) const;
			Vec3 GetAccumulatedScaleDelta( ) const; 

		private:
			LineIntersectionResult GetLineIntersectionResult( const Vec3& axisA, const Vec3& axisB, const Vec3& axisC, bool comparedSupportingAxes = true, bool overrideAxis = false, const Vec3& axisToUseAsPlaneNormal = Vec3(0.0f) );
			LineIntersectionResult GetLineIntersectionResultSingleAxis( const Vec3& axis );
			LineIntersectionResult GetLineIntersectionResultSingleAxisOverride( const Vec3& axis );
			void StoreIntersectionResultInformation( const LineIntersectionResult& result, TransformWidgetRenderableType type ); 

		private:
			EditorApp* mEditorApp = nullptr;
			EditorWidget* mActiveWidget = nullptr;
			Transform mWorldTransform;					
			TransformationMode mMode = TransformationMode::Translation;
			TransformWidgetRenderableType mType = TransformWidgetRenderableType::TranslationRightAxis;
			Vec3 mIntersectionStartPosition;
			Vec3 mImmutableIntersectionStartPosition;
			Vec3 mRootStartPosition;
			Vec3 mRootStartScale;
			Quaternion mStartRotation;
			Vec3 mDelta;
			f32 mAngleDelta;
			Quaternion mDeltaRotation;
			Vec2 mPreviousMouseCoords;
			bool mInteractingWithTransformWidget = true; 
			bool mEnabled = false;
			bool mSetPreviousAngle = false;
			f32 mPreviousAngle = 0.0f;
			TransformSpace mTransformSpace = TransformSpace::World; 

			Vec3 mTranslationSnap = Vec3( 0.0f );
			Vec3 mScaleSnap = Vec3( 0.0f );
			Vec3 mRotationSnap = Vec3( 0.0f );
			Vec3 mAccumulatedTranslationDelta = Vec3( 0.0f );
			Vec3 mAccumulatedScaleDelta = Vec3( 0.0f );
			Quaternion mAccumulatedRotationDelta = Quaternion( );

			Transform mRootTransform;
	}; 
}

#endif
