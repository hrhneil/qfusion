/*
Copyright (C) 2011 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"
#include "qmesa.h"

/*
=========================================================

VERTEX BUFFER OBJECTS

=========================================================
*/

typedef struct vbohandle_s
{
	unsigned int index;
	mesh_vbo_t *vbo;
	struct vbohandle_s *prev, *next;
} vbohandle_t;

#define MAX_MESH_VERTREX_BUFFER_OBJECTS 	8192

#define VBO_ARRAY_USAGE_FOR_TAG(tag) \
	(GLenum)((tag) == VBO_TAG_STREAM || (tag) == VBO_TAG_STREAM_STATIC_ELEMS ? GL_STREAM_DRAW_ARB : GL_STATIC_DRAW_ARB)
#define VBO_ELEM_USAGE_FOR_TAG(tag) \
	(GLenum)((tag) == VBO_TAG_STREAM_STATIC_ELEMS ? GL_STREAM_DRAW_ARB : GL_STATIC_DRAW_ARB)

static mesh_vbo_t r_mesh_vbo[MAX_MESH_VERTREX_BUFFER_OBJECTS];

static vbohandle_t r_vbohandles[MAX_MESH_VERTREX_BUFFER_OBJECTS];
static vbohandle_t r_vbohandles_headnode, *r_free_vbohandles;

static elem_t *r_vbo_tempelems;
static unsigned r_vbo_numtempelems;

static void *r_vbo_tempvsoup;
static size_t r_vbo_tempvsoupsize;

static int r_num_active_vbos;

static elem_t *R_VBOElemBuffer( unsigned numElems );
static void *R_VBOVertBuffer( unsigned numVerts, size_t vertSize );

/*
* R_InitVBO
*/
void R_InitVBO( void )
{
	int i;

	r_vbo_tempelems = NULL;
	r_vbo_numtempelems = 0;

	r_vbo_tempvsoup = NULL;
	r_vbo_tempvsoupsize = 0;

	r_num_active_vbos = 0;

	memset( r_mesh_vbo, 0, sizeof( r_mesh_vbo ) );
	memset( r_vbohandles, 0, sizeof( r_vbohandles ) );

	// link vbo handles
	r_free_vbohandles = r_vbohandles;
	r_vbohandles_headnode.prev = &r_vbohandles_headnode;
	r_vbohandles_headnode.next = &r_vbohandles_headnode;
	for( i = 0; i < MAX_MESH_VERTREX_BUFFER_OBJECTS; i++ ) {
		r_vbohandles[i].index = i;
		r_vbohandles[i].vbo = &r_mesh_vbo[i];
	}
	for( i = 0; i < MAX_MESH_VERTREX_BUFFER_OBJECTS - 1; i++ ) {
		r_vbohandles[i].next = &r_vbohandles[i+1];
	}
}

/*
* R_CreateMeshVBO
*
* Create two static buffer objects: vertex buffer and elements buffer, the real
* data is uploaded by calling R_UploadVBOVertexData and R_UploadVBOElemData.
*
* Tag allows vertex buffer objects to be grouped and released simultaneously.
*/
mesh_vbo_t *R_CreateMeshVBO( void *owner, int numVerts, int numElems, int numInstances,
	vattribmask_t vattribs, vbo_tag_t tag, vattribmask_t halfFloatVattribs )
{
	int i;
	size_t size;
	GLuint vbo_id;
	vbohandle_t *vboh = NULL;
	mesh_vbo_t *vbo = NULL;
	GLenum array_usage = VBO_ARRAY_USAGE_FOR_TAG(tag);
	GLenum elem_usage = VBO_ELEM_USAGE_FOR_TAG(tag);
	size_t vertexSize;
	vattribbit_t lmattrbit;

	if( !glConfig.ext.vertex_buffer_object )
		return NULL;

	if( !r_free_vbohandles )
		return NULL;

	if( !glConfig.ext.half_float_vertex ) {
		halfFloatVattribs = 0;
	}
	else {
		if( !(halfFloatVattribs & VATTRIB_POSITION_BIT) ) {
			halfFloatVattribs &= ~(VATTRIB_AUTOSPRITE_BIT);
		}

		halfFloatVattribs &= ~VATTRIB_COLORS_BITS;
		halfFloatVattribs &= ~VATTRIB_BONES_BITS;

		// TODO: convert quaternion component of instance_t to half-float
		// when uploading instances data
		halfFloatVattribs &= ~VATTRIB_INSTANCES_BITS;
	}

	vboh = r_free_vbohandles;
	vbo = &r_mesh_vbo[vboh->index];
	memset( vbo, 0, sizeof( *vbo ) );

	// vertex data
	vertexSize = 0;

	vertexSize += FLOAT_VATTRIB_SIZE(VATTRIB_POSITION_BIT, halfFloatVattribs) * 4;

	// normals data
	if( vattribs & VATTRIB_NORMAL_BIT )
	{
		assert( !(vertexSize & 3) );
		vbo->normalsOffset = vertexSize;
		vertexSize += FLOAT_VATTRIB_SIZE(VATTRIB_NORMAL_BIT, halfFloatVattribs) * 4;
	}

	// s-vectors (tangent vectors)
	if( vattribs & VATTRIB_SVECTOR_BIT )
	{
		assert( !(vertexSize & 3) );
		vbo->sVectorsOffset = vertexSize;
		vertexSize += FLOAT_VATTRIB_SIZE(VATTRIB_SVECTOR_BIT, halfFloatVattribs) * 4;
	}

	// texture coordinates
	if( vattribs & VATTRIB_TEXCOORDS_BIT )
	{
		assert( !(vertexSize & 3) );
		vbo->stOffset = vertexSize;
		vertexSize += FLOAT_VATTRIB_SIZE(VATTRIB_TEXCOORDS_BIT, halfFloatVattribs) * 2;
	}

	// lightmap texture coordinates
	lmattrbit = VATTRIB_LMCOORDS0_BIT;
	for( i = 0; i < MAX_LIGHTMAPS/2; i++ ) {
		if( !(vattribs & lmattrbit) ) {
			break;
		}
		assert( !(vertexSize & 3) );
		vbo->lmstOffset[i] = vertexSize;
		vbo->lmstSize[i] = vattribs & VATTRIB_LMCOORDS0_BIT<<1 ? 4 : 2;
		vertexSize += FLOAT_VATTRIB_SIZE(VATTRIB_LMCOORDS0_BIT, halfFloatVattribs) * vbo->lmstSize[i];
		lmattrbit = ( vattribbit_t )( ( vattribmask_t )lmattrbit << 2 );
	}

	// vertex colors
	if( vattribs & VATTRIB_COLOR0_BIT )
	{
		assert( !(vertexSize & 3) );
		vbo->colorsOffset[0] = vertexSize;
		vertexSize += sizeof( int );
	}

	// bones data for skeletal animation
	if( (vattribs & VATTRIB_BONES_BITS) == VATTRIB_BONES_BITS ) {
		assert( SKM_MAX_WEIGHTS == 4 );

		assert( !(vertexSize & 3) );
		vbo->bonesIndicesOffset = vertexSize;
		vertexSize += sizeof( int );

		assert( !(vertexSize & 3) );
		vbo->bonesWeightsOffset = vertexSize;
		vertexSize += sizeof( int );
	}

	// autosprites
	// FIXME: autosprite2 requires waaaay too much data for such a trivial
	// transformation..
	if( (vattribs & VATTRIB_AUTOSPRITE_BIT) == VATTRIB_AUTOSPRITE_BIT ) {
		assert( !(vertexSize & 3) );
		vbo->spritePointsOffset = vertexSize;
		vertexSize += FLOAT_VATTRIB_SIZE(VATTRIB_AUTOSPRITE_BIT, halfFloatVattribs) * 4;
	}

	size = vertexSize * numVerts;


	// instances data
	if( ( (vattribs & VATTRIB_INSTANCES_BITS) == VATTRIB_INSTANCES_BITS ) && 
		numInstances && glConfig.ext.instanced_arrays ) {
		assert( !(vertexSize & 3) );
		vbo->instancesOffset = size;
		size += numInstances * sizeof( GLfloat ) * 8;
	}

	// pre-allocate vertex buffer
	vbo_id = 0;
	qglGenBuffersARB( 1, &vbo_id );
	if( !vbo_id )
		goto error;
	vbo->vertexId = vbo_id;

	RB_BindArrayBuffer( vbo->vertexId );
	qglBufferDataARB( GL_ARRAY_BUFFER_ARB, size, NULL, array_usage );
	if( qglGetError () == GL_OUT_OF_MEMORY )
		goto error;

	vbo->arrayBufferSize = size;

	// pre-allocate elements buffer
	vbo_id = 0;
	qglGenBuffersARB( 1, &vbo_id );
	if( !vbo_id )
		goto error;
	vbo->elemId = vbo_id;

	size = numElems * sizeof( elem_t );
	RB_BindElementArrayBuffer( vbo->elemId );
	qglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, size, NULL, elem_usage );
	if( qglGetError () == GL_OUT_OF_MEMORY )
		goto error;

	vbo->elemBufferSize = size;

	r_free_vbohandles = vboh->next;

	// link to the list of active vbo handles
	vboh->prev = &r_vbohandles_headnode;
	vboh->next = r_vbohandles_headnode.next;
	vboh->next->prev = vboh;
	vboh->prev->next = vboh;

	r_num_active_vbos++;

	vbo->registrationSequence = rsh.registrationSequence;
	vbo->vertexSize = vertexSize;
	vbo->numVerts = numVerts;
	vbo->numElems = numElems;
	vbo->owner = owner;
	vbo->index = vboh->index + 1;
	vbo->tag = tag;
	vbo->halfFloatAttribs = halfFloatVattribs;

	return vbo;

error:
	if( vbo )
		R_ReleaseMeshVBO( vbo );

	RB_BindArrayBuffer( 0 );
	RB_BindElementArrayBuffer( 0 );

	return NULL;
}

/*
* R_TouchMeshVBO
*/
void R_TouchMeshVBO( mesh_vbo_t *vbo )
{
	vbo->registrationSequence = rsh.registrationSequence;
}

/*
* R_VBOByIndex
*/
mesh_vbo_t *R_GetVBOByIndex( int index )
{
	if( index >= 1 && index <= MAX_MESH_VERTREX_BUFFER_OBJECTS ) {
		return r_mesh_vbo + index - 1;
	}
	return NULL;
}

/*
* R_ReleaseMeshVBO
*/
void R_ReleaseMeshVBO( mesh_vbo_t *vbo )
{
	GLuint vbo_id;

	assert( vbo != NULL );

	if( vbo->vertexId ) {
		vbo_id = vbo->vertexId;
		qglDeleteBuffersARB( 1, &vbo_id );
	}

	if( vbo->elemId ) {
		vbo_id = vbo->elemId;
		qglDeleteBuffersARB( 1, &vbo_id );
	}

	if( vbo->index >= 1 && vbo->index <= MAX_MESH_VERTREX_BUFFER_OBJECTS ) {
		vbohandle_t *vboh = &r_vbohandles[vbo->index - 1];

		// remove from linked active list
		vboh->prev->next = vboh->next;
		vboh->next->prev = vboh->prev;

		// insert into linked free list
		vboh->next = r_free_vbohandles;
		r_free_vbohandles = vboh;

		r_num_active_vbos--;
	}

	memset( vbo, 0, sizeof( *vbo ) );
	vbo->tag = VBO_TAG_NONE;
}

/*
* R_GetNumberOfActiveVBOs
*/
int R_GetNumberOfActiveVBOs( void )
{
	return r_num_active_vbos;
}

/*
* R_FillVertexBuffer
*/
#define R_FillVertexBuffer(intype,outtype,in,size,stride,numVerts,out) \
R_FillVertexBuffer ## intype ## outtype(in,size,stride,numVerts,(void *)(out))

#define R_FillVertexBuffer_f(intype,outtype,conv) \
static void R_FillVertexBuffer ## intype ## outtype( intype *in, size_t size, \
	size_t stride, unsigned numVerts, outtype *out ) \
{ \
	size_t i, j; \
	for( i = 0; i < numVerts; i++ ) { \
		for( j = 0; j < size; j++ ) { \
			out[j] = conv( *in++ ); \
		} \
		out = ( outtype * )(( qbyte * )out + stride); \
	} \
}

R_FillVertexBuffer_f(float, float, );
R_FillVertexBuffer_f(float, GLhalfARB, _mesa_float_to_half);
R_FillVertexBuffer_f(int, int, );
#define R_FillVertexBuffer_float_or_half(gl_type,in,size,stride,numVerts,out) \
	do {\
		if( gl_type == GL_HALF_FLOAT ) { \
			R_FillVertexBuffer( float, GLhalfARB, in, size, stride, numVerts, out ); \
		} \
		else { \
			R_FillVertexBuffer( float, float, in, size, stride, numVerts, out ); \
		} \
	} while( 0 )

/*
* R_UploadVBOVertexData
*
* Uploads required vertex data to the buffer.
*
* Vertex attributes masked by halfFloatVattribs will use half-precision floats
* to save memory, if GL_ARB_half_float_vertex is available. Note that if
* VATTRIB_POSITION_BIT is not set, it will also reset bits for other positional 
* attributes such as autosprite pos and instance pos.
*/
vattribmask_t R_UploadVBOVertexData( mesh_vbo_t *vbo, int vertsOffset, 
	vattribmask_t vattribs, const mesh_t *mesh, vbo_hint_t hint )
{
	int i, j;
	unsigned numVerts;
	size_t vertSize;
	vattribmask_t errMask;
	vattribmask_t hfa;
	qbyte *data;

	assert( vbo != NULL );
	assert( mesh != NULL );

	if( !vbo || !vbo->vertexId ) {
		return 0;
	}

	errMask = 0;
	numVerts = mesh->numVerts;
	vertSize = vbo->vertexSize;

	hfa = vbo->halfFloatAttribs;
	data = R_VBOVertBuffer( numVerts, vertSize );

	RB_BindArrayBuffer( vbo->vertexId );

	// upload vertex xyz data
	if( vattribs & VATTRIB_POSITION_BIT ) {
		if( !mesh->xyzArray ) {
			errMask |= VATTRIB_POSITION_BIT;
		}
		else {
			R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_POSITION_BIT, hfa ), 
				mesh->xyzArray[0],
				4, vertSize, numVerts, data + 0 );
		}
	}

	// upload normals data
	if( vbo->normalsOffset && (vattribs & VATTRIB_NORMAL_BIT) ) {
		if( !mesh->normalsArray ) {
			errMask |= VATTRIB_NORMAL_BIT;
		} else {
			R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_NORMAL_BIT, hfa ), 
				mesh->normalsArray[0],
				4, vertSize, numVerts, data + vbo->normalsOffset );
		}
	}
	 
	// upload tangent vectors
	if( vbo->sVectorsOffset && ( ( vattribs & (VATTRIB_SVECTOR_BIT|VATTRIB_AUTOSPRITE2_BIT) ) == VATTRIB_SVECTOR_BIT ) ) {
		if( !mesh->sVectorsArray ) {
			errMask |= VATTRIB_SVECTOR_BIT;
		} else {
			R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_SVECTOR_BIT, hfa ), 
				mesh->sVectorsArray[0],
				4, vertSize, numVerts, data + vbo->sVectorsOffset );
		}
	}

	// upload texture coordinates
	if( vbo->stOffset && (vattribs & VATTRIB_TEXCOORDS_BIT) ) {
		if( !mesh->stArray ) {
			errMask |= VATTRIB_TEXCOORDS_BIT;
		} else {
			R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_TEXCOORDS_BIT, hfa ), 
				mesh->stArray[0],
				2, vertSize, numVerts, data + vbo->stOffset );
		}
	}

	// upload lightmap texture coordinates
	if( vbo->lmstOffset[0] && ( vattribs & VATTRIB_LMCOORDS0_BIT ) ) {
		int i;
		vattribbit_t lmattrbit;

		lmattrbit = VATTRIB_LMCOORDS0_BIT;

		for( i = 0; i < MAX_LIGHTMAPS/2; i++ ) {
			if( !(vattribs & lmattrbit) ) {
				break;
			}
			if( !mesh->lmstArray[i*2+0] ) {
				errMask |= lmattrbit;
				break;
			}

			R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_LMCOORDS0_BIT, hfa ), 
				mesh->lmstArray[i*2+0][0],
				2, vertSize, numVerts, data + vbo->lmstOffset[i] );

			if( vattribs & (lmattrbit<<1) ) {
				if( !mesh->lmstArray[i*2+1] ) {
					errMask |= lmattrbit<<1;
					break;
				}
				R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_LMCOORDS0_BIT, hfa ), 
					mesh->lmstArray[i*2+1][0],
					2, vertSize, numVerts, data + vbo->lmstOffset[i] + 2 * sizeof( float ) );
			}

			lmattrbit <<= 2;
		}
	}

	// upload vertex colors (although indices > 0 are never used)
	if( vbo->colorsOffset[0] && (vattribs & VATTRIB_COLOR0_BIT) ) {
		if( !mesh->colorsArray[0] ) {
			errMask |= VATTRIB_COLOR0_BIT;
		}
		else {
			R_FillVertexBuffer( int, int, 
				(int *)&mesh->colorsArray[0][0],
				1, vertSize, numVerts, data + vbo->colorsOffset[0] );
		}
	}

	// upload centre and radius for autosprites
	// this code assumes that the mesh has been properly pretransformed
	if( vbo->spritePointsOffset && ( (vattribs & VATTRIB_AUTOSPRITE2_BIT) == VATTRIB_AUTOSPRITE2_BIT ) ) {
		// for autosprite2 also upload vertices that form the longest axis
		// the remaining vertex can be trivially computed in vertex shader
		vec3_t vd[3];
		float d[3];
		int longest_edge = -1, longer_edge = -1, short_edge;
		float longest_dist = 0, longer_dist = 0;
		const int edges[3][2] = { { 1, 0 }, { 2, 0 }, { 2, 1 } };
		vec4_t centre[4];
		vec4_t axes[4];
		vec4_t *verts = mesh->xyzArray;
		elem_t *elems, temp_elems[6];
		int numQuads;
		size_t bufferOffset0 = vbo->spritePointsOffset;
		size_t bufferOffset1 = vbo->sVectorsOffset;

		if( hint == VBO_HINT_ELEMS_QUAD ) {
			numQuads = numVerts / 4;
		}
		else {
			assert( mesh->elems != NULL );
			if( !mesh->elems ) {
				numQuads = 0;
			} else {
				numQuads = mesh->numElems / 6;
			}
		}

		for( i = 0, elems = mesh->elems; i < numQuads; i++, elems += 6 ) {
			if( hint == VBO_HINT_ELEMS_QUAD ) {
				elem_t firstV = i * 4;

				temp_elems[0] = firstV;
				temp_elems[1] = firstV + 2 - 1;
				temp_elems[2] = firstV + 2;

				temp_elems[3] = firstV;
				temp_elems[4] = firstV + 3 - 1;
				temp_elems[5] = firstV + 3;

				elems = temp_elems;
			}

			// find the longest edge, the long edge and the short edge
			longest_edge = longer_edge = -1;
			longest_dist = longer_dist = 0;
			for( j = 0; j < 3; j++ ) {
				float len;

				VectorSubtract( verts[elems[edges[j][0]]], verts[elems[edges[j][1]]], vd[j] );
				len = VectorLength( vd[j] );
				if( !len ) {
					len = 1;
				}
				d[j] = len;

				if( longest_edge == -1 || longest_dist < len ) {
					longer_dist = longest_dist;
					longer_edge = longest_edge;
					longest_dist = len;
					longest_edge = j;
				} else if( longer_dist < len ) {
					longer_dist = len;
					longer_edge = j;
				}
			}

			short_edge = 3 - (longest_edge + longer_edge);
			if( short_edge > 2 ) {
				continue;
			}

			// centre
			VectorAdd( verts[elems[edges[longest_edge][0]]], verts[elems[edges[longest_edge][1]]], centre[0] );
			VectorScale( centre[0], 0.5, centre[0] );
			// radius
			centre[0][3] = d[longest_edge] * 0.5; // unused
			// right axis, normalized
			VectorScale( vd[short_edge], 1.0 / d[short_edge], vd[short_edge] );
			// up axis, normalized
			VectorScale( vd[longer_edge], 1.0 / d[longer_edge], vd[longer_edge] );

			NormToLatLong( vd[short_edge], &axes[0][0] );
			NormToLatLong( vd[longer_edge], &axes[0][2] );

			for( j = 1; j < 4; j++ ) {
				Vector4Copy( centre[0], centre[j] );
				Vector4Copy( axes[0], axes[j] );
			}

			R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_AUTOSPRITE_BIT, hfa ), 
				centre[0],
				4, vertSize, 4, data + bufferOffset0 );
			R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_SVECTOR_BIT, hfa ), 
				axes[0],
				4, vertSize, 4, data + bufferOffset1 );

			bufferOffset0 += 4 * vertSize;
			bufferOffset1 += 4 * vertSize;
		}
	}
	else if( vbo->spritePointsOffset && ( (vattribs & VATTRIB_AUTOSPRITE_BIT) == VATTRIB_AUTOSPRITE_BIT ) ) {
		vec4_t *verts;
		vec4_t centre[4];
		int numQuads = numVerts / 4;
		size_t bufferOffset = vbo->spritePointsOffset;

		for( i = 0, verts = mesh->xyzArray; i < numQuads; i++, verts += 4 ) {
			// centre
			for( j = 0; j < 3; j++ ) {
				centre[0][j] = (verts[0][j] + verts[1][j] + verts[2][j] + verts[3][j]) * 0.25;
			}
			// radius
			centre[0][3] = Distance( verts[0], centre[0] ) * 0.707106f;		// 1.0f / sqrt(2)

			for( j = 1; j < 4; j++ ) {
				Vector4Copy( centre[0], centre[j] );
			}

			R_FillVertexBuffer_float_or_half( FLOAT_VATTRIB_GL_TYPE( VATTRIB_AUTOSPRITE_BIT, hfa ), 
				centre[0],
				4, vertSize, 4, data + bufferOffset );

			bufferOffset += 4 * vertSize;
		}
	}

	if( vattribs & VATTRIB_BONES_BITS ) {
		if( vbo->bonesIndicesOffset ) {
			if( !mesh->blendIndices ) {
				errMask |= VATTRIB_BONESINDICES_BIT;
			}
			else {
				R_FillVertexBuffer( int, int, 
					(int *)&mesh->blendIndices[0],
					1, vertSize, numVerts, data + vbo->bonesIndicesOffset );
			}
		}
		if( vbo->bonesWeightsOffset ) {
			if( !mesh->blendWeights ) {
				errMask |= VATTRIB_BONESWEIGHTS_BIT;
			}
			else {
				R_FillVertexBuffer( int, int, 
					(int *)&mesh->blendWeights[0],
					1, vertSize, numVerts, data + vbo->bonesWeightsOffset );
			}
		}
	}

	qglBufferSubDataARB( GL_ARRAY_BUFFER_ARB, vertsOffset * vertSize, numVerts * vertSize, data );

	return errMask;
}

/*
* R_VBOElemBuffer
*/
static elem_t *R_VBOElemBuffer( unsigned numElems )
{
	if( numElems > r_vbo_numtempelems ) {
		if( r_vbo_numtempelems )
			R_Free( r_vbo_tempelems );
		r_vbo_numtempelems = numElems;
		r_vbo_tempelems = ( elem_t * )R_Malloc( sizeof( *r_vbo_tempelems ) * numElems );
	}

	return r_vbo_tempelems;
}

/*
* R_VBOVertBuffer
*/
static void *R_VBOVertBuffer( unsigned numVerts, size_t vertSize )
{
	size_t size = numVerts * vertSize;
	if( size > r_vbo_tempvsoupsize ) {
		if( r_vbo_tempvsoup )
			R_Free( r_vbo_tempvsoup );
		r_vbo_tempvsoupsize = size;
		r_vbo_tempvsoup = ( float * )R_Malloc( size );
	}
	return r_vbo_tempvsoup;
}

/*
* R_DiscardVBOVertexData
*/
void R_DiscardVBOVertexData( mesh_vbo_t *vbo )
{
	GLenum array_usage = VBO_ARRAY_USAGE_FOR_TAG(vbo->tag);

	if( vbo->vertexId ) {
		RB_BindArrayBuffer( vbo->vertexId );
		qglBufferDataARB( GL_ARRAY_BUFFER_ARB, vbo->arrayBufferSize, NULL, array_usage );
	}

}

/*
* R_DiscardVBOElemData
*/
void R_DiscardVBOElemData( mesh_vbo_t *vbo )
{
	GLenum elem_usage = VBO_ELEM_USAGE_FOR_TAG(vbo->tag);

	if( vbo->elemId ) {
		RB_BindElementArrayBuffer( vbo->elemId );
		qglBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, vbo->elemBufferSize, NULL, elem_usage );
	}
}

/*
* R_UploadVBOElemQuadData
*/
static int R_UploadVBOElemQuadData( mesh_vbo_t *vbo, int vertsOffset, int elemsOffset, int numVerts )
{
	int numElems;
	elem_t *ielems;

	assert( vbo != NULL );

	if( !vbo->elemId )
		return 0;

	numElems = (numVerts + numVerts + numVerts) / 2;
	ielems = R_VBOElemBuffer( numElems );

	R_BuildQuadElements( vertsOffset, numVerts, ielems );

	RB_BindElementArrayBuffer( vbo->elemId );
	qglBufferSubDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, elemsOffset * sizeof( elem_t ), 
		numElems * sizeof( elem_t ), ielems );

	return numElems;
}

/*
* R_UploadVBOElemTrifanData
*
* Builds and uploads indexes in trifan order, properly offsetting them for batching
*/
static int R_UploadVBOElemTrifanData( mesh_vbo_t *vbo, int vertsOffset, int elemsOffset, int numVerts )
{
	int numElems;
	elem_t *ielems;

	assert( vbo != NULL );

	if( !vbo->elemId )
		return 0;

	numElems = (numVerts - 2) * 3;
	ielems = R_VBOElemBuffer( numElems );

	R_BuildTrifanElements( vertsOffset, numVerts, ielems );

	RB_BindElementArrayBuffer( vbo->elemId );
	qglBufferSubDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, elemsOffset * sizeof( elem_t ), 
		numElems * sizeof( elem_t ), ielems );

	return numElems;
}

/*
* R_UploadVBOElemData
*
* Upload elements into the buffer, properly offsetting them (batching)
*/
void R_UploadVBOElemData( mesh_vbo_t *vbo, int vertsOffset, int elemsOffset, 
	const mesh_t *mesh, vbo_hint_t hint )
{
	int i;
	elem_t *ielems;

	assert( vbo != NULL );

	if( !vbo->elemId )
		return;

	if( hint == VBO_HINT_ELEMS_QUAD ) {
		R_UploadVBOElemQuadData( vbo, vertsOffset, elemsOffset, mesh->numVerts );
		return;
	}
	if( hint == VBO_HINT_ELEMS_TRIFAN ) {
		R_UploadVBOElemTrifanData( vbo, vertsOffset, elemsOffset, mesh->numVerts );
		return;
	}


	ielems = R_VBOElemBuffer( mesh->numElems );
	for( i = 0; i < mesh->numElems; i++ ) {
		ielems[i] = vertsOffset + mesh->elems[i];
	}

	RB_BindElementArrayBuffer( vbo->elemId );
	qglBufferSubDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, elemsOffset * sizeof( elem_t ), 
		mesh->numElems * sizeof( elem_t ), ielems );
}

/*
* R_UploadVBOInstancesData
*/
vattribmask_t R_UploadVBOInstancesData( mesh_vbo_t *vbo, int instOffset,
	int numInstances, instancePoint_t *instances )
{
	vattribmask_t errMask = 0;

	assert( vbo != NULL );

	if( !vbo->vertexId ) {
		return 0;
	}

	if(	!instances ) { 
		errMask |= VATTRIB_INSTANCES_BITS;
	}

	if( errMask ) {
		return errMask;
	}

	if( vbo->instancesOffset ) {
		qglBufferSubDataARB( GL_ARRAY_BUFFER_ARB, 
			vbo->instancesOffset + instOffset * sizeof( instancePoint_t ), 
			numInstances * sizeof( instancePoint_t ), instances );
	}

	return 0;
}

/*
* R_FreeVBOsByTag
*
* Release all vertex buffer objects with specified tag.
*/
void R_FreeVBOsByTag( vbo_tag_t tag )
{
	mesh_vbo_t *vbo;
	vbohandle_t *vboh, *next, *hnode;

	if( !r_num_active_vbos ) {
		return;
	}

	hnode = &r_vbohandles_headnode;
	for( vboh = hnode->prev; vboh != hnode; vboh = next ) {
		next = vboh->prev;
		vbo = &r_mesh_vbo[vboh->index];

		if( vbo->tag == tag ) {
			R_ReleaseMeshVBO( vbo );
		}
	}
}

/*
* R_FreeUnusedVBOs
*/
void R_FreeUnusedVBOs( void )
{
	mesh_vbo_t *vbo;
	vbohandle_t *vboh, *next, *hnode;

	if( !r_num_active_vbos ) {
		return;
	}

	hnode = &r_vbohandles_headnode;
	for( vboh = hnode->prev; vboh != hnode; vboh = next ) {
		next = vboh->prev;
		vbo = &r_mesh_vbo[vboh->index];

		if( vbo->registrationSequence != rsh.registrationSequence ) {
			R_ReleaseMeshVBO( vbo );
		}
	}
}

/*
* R_ShutdownVBO
*/
void R_ShutdownVBO( void )
{
	mesh_vbo_t *vbo;
	vbohandle_t *vboh, *next, *hnode;

	if( !r_num_active_vbos ) {
		return;
	}

	hnode = &r_vbohandles_headnode;
	for( vboh = hnode->prev; vboh != hnode; vboh = next ) {
		next = vboh->prev;
		vbo = &r_mesh_vbo[vboh->index];

		R_ReleaseMeshVBO( vbo );
	}

	if( r_vbo_tempelems ) {
		R_Free( r_vbo_tempelems );
	}
	r_vbo_numtempelems = 0;
}
