#include "Graphics/PointLight.h"
#include "Graphics/GraphicsScene.h"

namespace Enjon 
{ 
	//============================================================================================================================

	void PointLight::ExplicitConstructor()
	{
		mPosition 	= Vec3(0.0f, 0.0f, 0.0f);
		mColor 		= RGBA32_White(); 
		mAttenuationRate = 0.5f;
		mIntensity = 100.0f;
		mRadius = 100.0f;
	}

	//============================================================================================================================

	PointLight::PointLight(Vec3& position, float attenuationRate, ColorRGBA32& color, float intensity, float radius)
	{
		mPosition = position;
		mAttenuationRate = attenuationRate;
		mColor = color;
		mIntensity = intensity;	
		mRadius = radius;
	}

	//============================================================================================================================

	void PointLight::ExplicitDestructor()
	{
		if (mGraphicsScene != nullptr)
		{
			mGraphicsScene->RemovePointLight(this);
		}
	}

	//============================================================================================================================

	void PointLight::SetColor(ColorRGBA32& color)
	{
		mColor = color;
	}

	//============================================================================================================================

	void PointLight::SetIntensity(float intensity)
	{
		mIntensity = intensity;	
	}

	void PointLight::SetGraphicsScene(GraphicsScene* scene)
	{
		mGraphicsScene = scene;
	}

	void PointLight::SetPosition(Vec3& position)
	{
		mPosition = position;
	}

	void PointLight::SetAttenuationRate(float rate)
	{
		mAttenuationRate = rate;
	}

	void PointLight::SetRadius(float radius)
	{
		mRadius = radius;
	}

	//==============================================================================================

	GraphicsScene* PointLight::GetGraphicsScene( ) const
	{
		return mGraphicsScene;
	}

	//============================================================================================== 

	float PointLight::GetAttenuationRate() const 
	{ 
		return mAttenuationRate; 
	}

	//============================================================================================== 
	
	float PointLight::GetRadius() const
	{ 
		return mRadius; 
	}

	//============================================================================================== 

	Vec3 PointLight::GetPosition() const 	
	{ 
		return mPosition; 
	}

	//============================================================================================== 

	ColorRGBA32 PointLight::GetColor() const  
	{ 
		return mColor; 
	}

	//============================================================================================== 

	float PointLight::GetIntensity() const
	{ 
		return mIntensity; 
	}

	//============================================================================================== 
}