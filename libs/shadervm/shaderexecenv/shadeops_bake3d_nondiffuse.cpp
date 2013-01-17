// AUTHOR: KARSTEN DAEMEN
// THIS CLASS

#ifdef AQSIS_SYSTEM_WIN32
#include <io.h>
#endif

#include	<math.h>
#include	<stdio.h>
#include	<iostream>
#include	<string>
#include	<cstring>
#include	<Partio.h>
#include	<OpenEXR/ImathVec.h>

#include	<aqsis/util/autobuffer.h>
#include	<aqsis/util/logging.h>
#include	<aqsis/math/math.h>
#include	<aqsis/core/ilightsource.h>

#include	"../../pointrender/MicroBuf.h"
#include	"../../pointrender/diffuse/DiffusePointOctreeCache.h"
#include	"../../pointrender/RadiosityIntegrator.h"
#include	"../../pointrender/microbuf_proj_func.h"
#include	"../../pointrender/nondiffuse/NonDiffusePoint.h"

#include	"shaderexecenv.h"

namespace Aqsis {

using Imath::V3f;
using Imath::C3f;
using std::vector;


/**
 * This function transforms the incoming radiance Cl from a direction L to the outgoing
 * radiosity on the microbuffer.
 *
 * \param outgoingRadBuffer	- The microbuffer holding the outgoing radiance.
 * \param phongExponent 	- The phongExponent of the surface.
 */
void toOutgoingRadiance(MicroBuf& outgoingRadBuffer, V3f N, V3f L, C3f Cl,
		int phongExponent);

/**
 * Shadeop to bake non diffuse point cloud from diffuse point cloud.
 *
 *
 * \param ptc		- The name of the pointcloud file.
 * \param channels	- Not used, is here for PRMan compatibility
 * \param position	- Vertex positions (IqShaderData* is organised as a list of	vectors).
 * \param normal 	- Normals (IqShaderData* is organised as a list of vectors).
 * \param result 	- 0 or 1, depending on failure or success
 * \param pShader 	- Unused.
 * \param cParams	- Number of extra user parameters.
 * \param apParams	- List of extra parameters to control. Parameter name is always
 * 					followed by the value.
 *
 */

void CqShaderExecEnv::SO_bake3d_nondiffuse(IqShaderData* ptc,
												IqShaderData* P,
												IqShaderData* N,
												IqShaderData* Cs,
												IqShaderData* area,
												IqShaderData* result,
												IqShader* pShader,
												TqInt cParams,
												IqShaderData** apParams) {

	// Check if there's a RenderContext.
	// (is this function called during rendering?)
	if (!getRenderContext()) {
		return;
	}

	/*
	 * Variables
	 */

	// Resolution of the microbuffer face.
	int faceRes = 10;
	// Default coordinate system to use
	CqString coordSystem = "world";
	// Holding the categories of the lights to use.
	IqShaderData* category = NULL;
	// Phong exponent
	int phong = -1;

	/*
	 * Parsing the parameters ...
	 */
	CqString paramName;
	for (int i = 0; i < cParams; i += 2) {
		if (apParams[i]->Type() == type_string) {
			apParams[i]->GetString(paramName);
			IqShaderData* paramValue = apParams[i + 1];
			if (paramName == "coordsystem") {
				if (paramValue->Type() == type_string)
					paramValue->GetString(coordSystem);
			} else if (paramName == "microbufres") {
				if (paramValue->Type() == type_float) {
					float res = 10;
					paramValue->GetFloat(res);
					faceRes = std::max(1, static_cast<int> (res));
				}
			} else if (paramName == "phong") {
				if (paramValue->Type() == type_float) {
					float exponent = -1;
					paramValue->GetFloat(exponent);
					phong = std::max(0, static_cast<int> (exponent));
				}
			} else if (paramName == "_category") {
				category = paramValue;
			}
		}
	}

	/**
	 * Apply necessary transformations.
	 */

	// Compute current transform to appropriate space.
	// During rasterisation, the coordinates are not real world coordinates.
	CqMatrix positionTrans;
	getRenderContext()->matSpaceToSpace("current", coordSystem.c_str(),
			pShader->getTransform(), pTransform().get(), 0, positionTrans);
	CqMatrix normalTrans = normalTransform(positionTrans);


	/**
	 * Get the incoming licht radiosity and direction to use afterwards while calculating
	 * the hemispheres.
	 */

	const CqBitVector& RS = RunningState();

	// Create the IqShaderData to hold the coneangle for the illuminace shadeop.
	IqShaderData* pDefAngle = pShader->CreateTemporaryStorage(type_float, class_uniform);

	// Check if all parameters are assigned.
	if (NULL == pDefAngle) {
		Aqsis::log() << error << "bake_nondiffuse: Not able to reserve memory for "
				<< "parameter to illuminance call." << std::endl;
		return;
	}

	//Initiate the possible parameters.
	pDefAngle->SetFloat(coneAngle);


	// If the illuminance cache is already OK, then we don't need to bother filling
	// in the illuminance parameters.
	if (!m_IlluminanceCacheValid) {
		ValidateIlluminanceCache(NULL, N, pShader);
	}

	// Start iterating over all the lightsources in the scene that fall within the cone.
	int npoints = shadingPointCount();
	int nLights = 0;
	V3f* memL;
	C3f* memCl;
	if (SO_init_illuminance()) {

		int j = 0; // Light index

		// Formula to calculate the number of non ambient lights.
		nLights = m_pAttributes ->cLights() - m_li;

		// Reserve memory for saving the light variables.
		float nan = std::numeric_limits<float>::quiet_NaN();
		vector<V3f> vecMemL(npoints * nLights, V3f(nan, nan, nan));
		vector<C3f> vecMemCl(npoints * nLights, C3f(nan, nan, nan));
		memL = &vecMemL[0];
		memCl = &vecMemCl[0];

		do {

			// SO_illuminance sets the current state to whether the lightsource illuminates the points or not.
			SO_illuminance(category, NULL, N, pDefAngle, NULL);

			PushState();
			GetCurrentState();

			for (int igrid = 0; igrid < npoints; ++igrid) {
				if (RS.Value(igrid)) {

					// Get the Light Color.
					CqColor Clval;
					Cl() ->GetColor(Clval, igrid);
					C3f Clval2(Clval.r(), Clval.g(), Clval.b());
					memCl[igrid * nLights + j] = Clval2;

					// Get the Light vector.
					CqVector3D Lval;
					L()->GetVector(Lval, igrid);
					Lval = Lval.Unit();
					V3f Lval2(Lval.x(), Lval.y(), Lval.z());
					memL[igrid * nLights + j] = Lval2;
				}
			}
			PopState();
			j++;
			// SO_advance_illuminance returns TRUE if there are any more non ambient lightsources.
		} while (SO_advance_illuminance());

		// Delete the IqShaderData* holding the coneangle, no longer needed.
		pShader->DeleteTemporaryStorage(pDefAngle);

		/**
		 * Bake the outgoing non diffuse surphels.
		 */

		// number of floats in hemisphere.
		int hemiSize = 6 * faceRes * faceRes * 3;

		// Create the IqShaderData* that will hold the hemispheres to be baked.
		IqShaderData* H = pShader->CreateVariableArray(type_float,
				class_varying, CqString("_hemi"), TqInt(hemiSize),
				IqShaderData::Temporary);

		if (H == NULL) {
			Aqsis::log() << error
					<< "bake_nondiffuse: Not able to reserve memory "
					<< "for parameter to bake3d call." << std::endl;
			return;
		}

		for (int i = 0; i < hemiSize; i++)
			H->ArrayEntry(i)->SetSize(npoints);

#pragma omp parallel
		{
			// Define the microbuffer to hold the outgoing radiance.
			RadiosityIntegrator outgoingRadIntegrator(faceRes);

#pragma omp for
			// For every shading point in this shading grid do ...
			for (int igrid = 0; igrid < npoints; ++igrid) {
				if (RS.Value(igrid)) {

					// Get the Normal vector.
					CqVector3D Nval;
					N->GetVector(Nval, igrid);
					Nval = normalTrans * Nval;
					V3f Nval2(Nval.x(), Nval.y(), Nval.z());

					// Get the Incoming vector.
					CqVector3D Ival;
					I()->GetVector(Ival, igrid);
					V3f Ival2(Ival.x(), Ival.y(), Ival.z());

					// Get the Position vector.
					CqVector3D Pval;
					P->GetVector(Pval, igrid);
					Pval = positionTrans * Pval;
					V3f Pval2(Pval.x(), Pval.y(), Pval.z());

					outgoingRadIntegrator.clear();
					MicroBuf& microbuf = outgoingRadIntegrator.microBuf();
					for (int j = 0; j < nLights; j++) {
						V3f Lval2 = memL[igrid * nLights + j];
						C3f Clval2 = memCl[igrid * nLights + j];

						if (!isnan(Lval2.x)) {
							toOutgoingRadiance(microbuf, Nval2,	Lval2, Clval2, phong);
						}
					}

					// Add the hemisphere to the IqShaderData* that will be passed to bake3d.
					float* data = microbuf.face(0);
					float nondiffuse[7+microbuf.size()*3];
					for (int j=7, i=0, entry=0; entry < microbuf.size()*3; entry+=3, i+=5, j+=3) {
						H->ArrayEntry(entry)->SetFloat(data[i+2],igrid);
						H->ArrayEntry(entry+1)->SetFloat(data[i+3],igrid);
						H->ArrayEntry(entry+2)->SetFloat(data[i+4],igrid);


						nondiffuse[j] = data[i+2];
						nondiffuse[j+1] = data[i+3];
						nondiffuse[j+2] = data[i+4];

//						H->ArrayEntry(entry)->SetFloat(1,igrid);
//						H->ArrayEntry(entry+1)->SetFloat(1,igrid);
//						H->ArrayEntry(entry+2)->SetFloat(1,igrid);
					}




					// Return the first bounce reflection as an indication of the quality ...
					Ival2.setValue(-Ival.x(), -Ival.y(), -Ival.z());
//					C3f col = microbuf.getRadiosityInDir(Ival2);
					NonDiffusePoint point(&nondiffuse[0],faceRes);
					C3f col = point.getInterpolatedRadiosityInDir(Ival2);

					result->SetColor(CqColor(col.x, col.y, col.z), igrid);

				} // endif varying
			} // endfor shadingpoints
		}

		/*
		 * Create the various parameters for the call to bake3d.
		 */

		// The name parameter of the hemispheres.
		IqShaderData* nameH = pShader->CreateTemporaryStorage(type_string,
				class_uniform);

		// The var  + param name indicating interpolation.
		IqShaderData* nameInterpolate = pShader->CreateTemporaryStorage(
				type_string, class_uniform);
		IqShaderData* interpolate = pShader->CreateTemporaryStorage(type_bool,
				class_uniform);

		// The name parameter of the area.
		IqShaderData* nameArea = pShader->CreateTemporaryStorage(type_string,
				class_uniform);

		// The var holding the end result of Bake3d
		IqShaderData* resultBake3d = pShader->CreateTemporaryStorage(
				type_float, class_varying);

		// Check if all parameters are assigned.
		if (NULL == nameH || NULL == nameInterpolate || NULL == interpolate
				|| NULL == nameArea || NULL == resultBake3d) {
			Aqsis::log() << error << "bake_nondiffuse: Not able to reserve"
					<< " memory for parameters to bake3d call." << std::endl;
			return;
		}

		/*
		 * Initiate these parameters.
		 */

		// Initiate the name varof hemispheres.
		nameH->SetString("_hemi");

		// Initiate the var indicating interpolate + name.
		nameInterpolate->SetString("interpolate");
		interpolate->SetBool(false);

		// Initiate the name var of area
		nameArea->SetString("_area");

		// Initiare the size of Result.
		resultBake3d->SetSize(npoints);

		// Put the right parameters in the array of apParams.
		TqInt cParamsNew = 6;
		IqShaderData* apParamsNew[cParamsNew];
		apParamsNew[0] = nameH;
		apParamsNew[1] = H;
		apParamsNew[2] = nameArea;
		apParamsNew[3] = area;
		apParamsNew[4] = nameInterpolate;
		apParamsNew[5] = interpolate;

		/*
		 *  Make the call to bake3d.
		 */

		SO_bake3d(ptc, NULL, P, N, resultBake3d, pShader, cParamsNew,
				apParamsNew);

		/*
		 * Delete all the temporary parameters afterwards.
		 */
		pShader->DeleteTemporaryStorage(resultBake3d);
		pShader->DeleteTemporaryStorage(nameArea);
		pShader->DeleteTemporaryStorage(interpolate);
		pShader->DeleteTemporaryStorage(nameInterpolate);
		pShader->DeleteTemporaryStorage(nameH);
		pShader->DeleteTemporaryStorage(H);
	}
}

/**
 * This function transforms the incoming radiance Cl from a direction L to the outgoing
 * radiosity on the microbuffer.
 */
void toOutgoingRadiance(MicroBuf& outgoingRadBuffer, V3f N, V3f L, C3f Cl,
		int phong) {

	V3f R = -L - 2 * (dot(-L, N)) * N;

	for (int fo = MicroBuf::Face_begin; fo < MicroBuf::Face_end; ++fo) {
		const float* oFace = outgoingRadBuffer.face(fo);
		for (int vo = 0; vo < outgoingRadBuffer.res(); ++vo) {
			for (int uo = 0; uo < outgoingRadBuffer.res(); ++uo, oFace
					+= outgoingRadBuffer.nchans()) {

				V3f direction = outgoingRadBuffer.rayDirection(fo, uo, vo);
				float dotp = dot(direction, N);
				if (dotp > 0) {
					float size = outgoingRadBuffer.pixelSize(uo, vo);
					// Calculate phong factor
					float phongFactor = pow(std::max(0.0f, dot(R, direction)),	phong);
					float normPhongFactor = phongFactor*((phong+1)/(2*M_PI));

					C3f* rad = (C3f*) (oFace + 2);
					rad->x += Cl.x * normPhongFactor * dotp;
					rad->y += Cl.y * normPhongFactor * dotp;
					rad->z += Cl.z * normPhongFactor * dotp;
				}
			}
		}
	}
}

} // namespace Aqsis
