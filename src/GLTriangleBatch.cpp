/*
 *  GLTriangleBatch.cpp
 *

Copyright (c) 2007-2023, Richard S. Wright Jr.
All rights reserved.

Redistribution and use in source, with or without modification, 
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list 
of conditions and the following disclaimer.

Neither the name of Richard S. Wright Jr. nor the names of other contributors may be used 
to endorse or promote products derived from this software without specific prior 
written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED 
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


 *  This class allows you to simply add triangles as if this class were a 
 *  container. The AddTriangle() function searches the current list of triangles
 *  and determines if the vertex/normal/texcoord is a duplicate. If so, it addes
 *  an entry to the index array instead of the list of vertices.
 *  When finished, call EndMesh() to free up extra unneeded memory that is reserved
 *  as workspace when you call BeginMesh().
 *
 *  This class can easily be extended to contain other vertex attributes, and to 
 *  save itself and load itself from disk (thus forming the beginnings of a custom
 *  model file format).
 *
 */
 
 
#include "GLTools.h"
#include "GLTriangleBatch.h"
#include <assert.h>


// Highest 64-bit address. No memory allocation would return this address
#define NOT_VALID_BUT_USED 0xFFFFFFFFFFFFFFFF

///////////////////////////////////////////////////////////
// Constructor, does what constructors do... set everything to zero or NULL
GLTriangleBatch::GLTriangleBatch(void)
    {
    nMaxIndexes = 0;
    nNumIndexes = 0;
    nNumVerts = 0;
    
    bMadeStuff = false;
	boundingSphereRadius = 0.0f;
    }
    
////////////////////////////////////////////////////////////
// Free any dynamically allocated memory. For those C programmers
// coming to C++, it is perfectly valid to delete a NULL pointer.
GLTriangleBatch::~GLTriangleBatch(void)
    {
    // Just in case these still are allocated when the object is destroyed
    // End does this and leaves the pointers not NULL as a flag as to which
    // ones were used. Don't uncoment this....
    if(pIndexes != (GLushort*)NOT_VALID_BUT_USED)
        delete [] pIndexes;

    if(pVerts != (M3DVector3f*)NOT_VALID_BUT_USED)
        delete [] pVerts;

    if(pNorms != (M3DVector3f*)NOT_VALID_BUT_USED)
        delete [] pNorms;

    if(pTexCoords != (M3DVector2f*)NOT_VALID_BUT_USED)
       delete [] pTexCoords;
    
    // Delete buffer objects
    if(bMadeStuff) {
#if defined ( ANDROID_NDK ) || defined ( __EMSCRIPTEN__ )
		glDeleteVertexArraysOES(1, &vertexArrayBufferObject);
#else
		glDeleteVertexArrays(1, &vertexArrayBufferObject);
#endif

        glDeleteBuffers(4, bufferObjects);
        }
    }
    
////////////////////////////////////////////////////////////
// Start assembling a mesh. You need to specify a maximum amount
// of indexes that you expect. The EndMesh will clean up any uneeded
// memory. This is far better than shreading your heap with STL containers...
// At least that's my humble opinion.
void GLTriangleBatch::BeginMesh(GLuint nMaxVerts)
    {
#ifdef QT_IS_AVAILABLE
    initializeOpenGLFunctions();
#endif

    // Just in case this gets called more than once...
    if(pIndexes != (GLushort*)NOT_VALID_BUT_USED)
        delete [] pIndexes;

    if(pVerts != (M3DVector3f*)NOT_VALID_BUT_USED)
        delete [] pVerts;

    if(pNorms != (M3DVector3f*)NOT_VALID_BUT_USED)
        delete [] pNorms;

    if(pTexCoords != (M3DVector2f*)NOT_VALID_BUT_USED)
       delete [] pTexCoords;
    
    nMaxIndexes = nMaxVerts;
    nNumIndexes = 0;
    nNumVerts = 0;
    
    // Pre-allocate new blocks. In reality, the other arrays will be
    // much shorter than the index array
    pIndexes = new GLushort[nMaxIndexes];
    pVerts = new M3DVector3f[nMaxIndexes];
    pNorms = new M3DVector3f[nMaxIndexes];
    pTexCoords = new M3DVector2f[nMaxIndexes];
    }
  
/////////////////////////////////////////////////////////////////
// Add a triangle to the mesh. This searches the current list for identical
// (well, almost identical - these are floats you know...) verts. If one is found, it
// is added to the index array. If not, it is added to both the index array and the vertex
// array grows by one as well.
// nCheckRange allows for optimizing the search
void GLTriangleBatch::AddTriangle(M3DVector3f verts[3], M3DVector3f vNorms[3], M3DVector2f vTexCoords[3], float epsilon, int nCheckRange)
    {
    // Silently fail unless in debug mode
    if(nNumIndexes >= nMaxIndexes) {
        assert(false);
        return;
        }

    // First thing we do is make sure the normals are unit length!
    // It's almost always a good idea to work with pre-normalized normals
    if(vNorms != nullptr) {
        m3dNormalizeVector3(vNorms[0]);
        m3dNormalizeVector3(vNorms[1]);
        m3dNormalizeVector3(vNorms[2]);
        }

	// If we.. even once, set texture to NULL, then nothing in the batch has texture coordinates
    if(vTexCoords == nullptr && pTexCoords != nullptr) {
		delete [] pTexCoords;
        pTexCoords = nullptr;
		}
        
    // Ditto for normals
    if(vNorms == nullptr && pNorms != nullptr) {
        delete [] pNorms;
        pNorms = nullptr;
        }

    // Allow not checking EVERY vertex
    int nSearchStart = nNumVerts - nCheckRange;
    if(nSearchStart < 0)
        nSearchStart = 0;

    // Search for match - triangle consists of three verts
    for(GLuint iVertex = 0; iVertex < 3; iVertex++) // This is our new triangle
        {
        GLuint iMatch = 0;
        for(iMatch = nSearchStart; iMatch < nNumVerts; iMatch++)   // This is all the triangles that came before
            {
            // We have vertexes, texture coordinates, and normals
			if(pTexCoords && pNorms) {
				if(m3dCloseEnough(pVerts[iMatch][0], verts[iVertex][0], epsilon) &&
				   m3dCloseEnough(pVerts[iMatch][1], verts[iVertex][1], epsilon) &&
				   m3dCloseEnough(pVerts[iMatch][2], verts[iVertex][2], epsilon) &&
					   
				   // AND the Normal is the same...
				   m3dCloseEnough(pNorms[iMatch][0], vNorms[iVertex][0], epsilon) &&
				   m3dCloseEnough(pNorms[iMatch][1], vNorms[iVertex][1], epsilon) &&
				   m3dCloseEnough(pNorms[iMatch][2], vNorms[iVertex][2], epsilon) &&
					   
					// And Texture is the same...
					m3dCloseEnough(pTexCoords[iMatch][0], vTexCoords[iVertex][0], epsilon) &&
					m3dCloseEnough(pTexCoords[iMatch][1], vTexCoords[iVertex][1], epsilon))
					{
					// Then add the index only
					pIndexes[nNumIndexes] = iMatch;
					nNumIndexes++;
					break;
					}
				}

            // We just have vertexes and normals, no texture
            if(pNorms && pTexCoords == NULL) {
                if(m3dCloseEnough(pVerts[iMatch][0], verts[iVertex][0], epsilon) &&
                   m3dCloseEnough(pVerts[iMatch][1], verts[iVertex][1], epsilon) &&
                   m3dCloseEnough(pVerts[iMatch][2], verts[iVertex][2], epsilon) &&
                       
                   // AND the Normal is the same...
                   m3dCloseEnough(pNorms[iMatch][0], vNorms[iVertex][0], epsilon) &&
                   m3dCloseEnough(pNorms[iMatch][1], vNorms[iVertex][1], epsilon) &&
                   m3dCloseEnough(pNorms[iMatch][2], vNorms[iVertex][2], epsilon))					   
                    {
                    // Then add the index only
                    pIndexes[nNumIndexes] = iMatch;
                    nNumIndexes++;
                    break;
                    }
                }
                 
            // We have vertexes, texture coordinates, and no normals
            if(pTexCoords && pNorms == NULL) {
				if(m3dCloseEnough(pVerts[iMatch][0], verts[iVertex][0], epsilon) &&
				   m3dCloseEnough(pVerts[iMatch][1], verts[iVertex][1], epsilon) &&
				   m3dCloseEnough(pVerts[iMatch][2], verts[iVertex][2], epsilon) &&
					   
					// And Texture is the same...
					m3dCloseEnough(pTexCoords[iMatch][0], vTexCoords[iVertex][0], epsilon) &&
					m3dCloseEnough(pTexCoords[iMatch][1], vTexCoords[iVertex][1], epsilon))
					{
					// Then add the index only
					pIndexes[nNumIndexes] = iMatch;
					nNumIndexes++;
					break;
					}
				}
                             
            // Just verts
            if(pNorms == NULL && pTexCoords == NULL) {
                if(m3dCloseEnough(pVerts[iMatch][0], verts[iVertex][0], epsilon) &&
                   m3dCloseEnough(pVerts[iMatch][1], verts[iVertex][1], epsilon) &&
                   m3dCloseEnough(pVerts[iMatch][2], verts[iVertex][2], epsilon))                       
                    {
                    // Then add the index only
                    pIndexes[nNumIndexes] = iMatch;
                    nNumIndexes++;
                    break;
                    }
                }   
            }
            
        // No match for this vertex, add to end of list
        if(iMatch == nNumVerts && nNumVerts < nMaxIndexes && nNumIndexes < nMaxIndexes)
            {
            // Always have verts
            memcpy(pVerts[nNumVerts], verts[iVertex], sizeof(M3DVector3f));
            
            // If we have normals
            if(pNorms)
                memcpy(pNorms[nNumVerts], vNorms[iVertex], sizeof(M3DVector3f));
            
            // if we have texture coordinates
            if(pTexCoords)
                memcpy(pTexCoords[nNumVerts], vTexCoords[iVertex], sizeof(M3DVector2f));
            
            pIndexes[nNumIndexes] = nNumVerts;
            nNumIndexes++; 
            nNumVerts++;
            }   
        }
    }
    

//////////////////////////////////////////////////////////////////
// Compact the data. This is a nice utility, but you should really
// save the results of the indexing for future use if the model data
// is static (doesn't change).
void GLTriangleBatch::End(void)
    {
    bMadeStuff = true;

    // Find the radius of the smallest sphere that would enclose the model
    // This is useful for some things.
    boundingSphereRadius = 0.0f;
    for(unsigned int i = 0; i < nNumVerts; i++) {
        GLfloat r = m3dGetVectorLengthSquared3(pVerts[i]);
        if(r > boundingSphereRadius)
            boundingSphereRadius = r;
        }
    boundingSphereRadius = sqrt(boundingSphereRadius);
    
    // Create the buffer objects - might need as many as four
    glGenBuffers(4, bufferObjects);
#if defined ( ANDROID_NDK ) || defined ( __EMSCRIPTEN__ )
	glGenVertexArraysOES(1, &vertexArrayBufferObject);
	glBindVertexArrayOES(vertexArrayBufferObject);
#else
    glGenVertexArrays(1, &vertexArrayBufferObject);
    glBindVertexArray(vertexArrayBufferObject);
#endif

    // Copy data to GPU memory
    // Vertex data
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[VERTEX_DATA]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*nNumVerts*3, pVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(GLT_ATTRIBUTE_VERTEX, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(GLT_ATTRIBUTE_VERTEX);
    delete [] pVerts;
    pVerts = (M3DVector3f*)NOT_VALID_BUT_USED;


    // Normal data
    if(pNorms) {
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[NORMAL_DATA]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*nNumVerts*3, pNorms, GL_STATIC_DRAW);
        glVertexAttribPointer(GLT_ATTRIBUTE_NORMAL, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(GLT_ATTRIBUTE_NORMAL);
        delete [] pNorms;
        pNorms = (M3DVector3f*)NOT_VALID_BUT_USED;
        }
    else {
        delete [] pNorms;
        pNorms = nullptr;
        }


    // Texture coordinates
    if(pTexCoords) {
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[TEXTURE_DATA]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*nNumVerts*2, pTexCoords, GL_STATIC_DRAW);
        glVertexAttribPointer(GLT_ATTRIBUTE_TEXTURE0, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(GLT_ATTRIBUTE_TEXTURE0);
        delete [] pTexCoords;
        pTexCoords = (M3DVector2f *)NOT_VALID_BUT_USED;
        }
        
    // Indexes
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[INDEX_DATA]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort)*nNumIndexes, pIndexes, GL_STATIC_DRAW);
    delete [] pIndexes;
    pIndexes = (GLushort*)NOT_VALID_BUT_USED;

#if defined ( ANDROID_NDK ) || defined ( __EMSCRIPTEN__ )
	glBindVertexArrayOES(0);
#else
	glBindVertexArray(0);
#endif

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);	// Note: This should NOT be necessary, it should be captured
										// in the vertex array object binding state. I believe this is a
										// bug in iOS's OpenGL implementation, and at it is simply redudant
										// in other implementations/platforms
    }

//////////////////////////////////////////////////////////////////////////
// Submit...
void GLTriangleBatch::Draw(void)
    {
    if(nNumIndexes <= 0)
        return;
#if defined ( ANDROID_NDK ) || defined ( __EMSCRIPTEN__ )
	glBindVertexArrayOES(vertexArrayBufferObject);
#else
	glBindVertexArray(vertexArrayBufferObject);
#endif
    glDrawElements(GL_TRIANGLES, nNumIndexes, GL_UNSIGNED_SHORT, 0);
    }

////////////////////////////////////////////////////////////////////////
// Save the mesh into the already open file stream
bool GLTriangleBatch::SaveMesh(FILE *pFile)
    {
	(void)pFile;
/*    // Header contains...
    fwrite(&nNumIndexes, sizeof(GLuint), 1, pFile);
    fwrite(&nNumVerts, sizeof(GLuint), 1, pFile);
    fwrite(&boundingSphereRadius, sizeof(GLuint), 1, pFile);
    
    
//    printf("Unique Verts: %d\r\nTriangles: %d\r\n\r\n", nNumVerts, nNumIndexes);

    // Indexes
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[INDEX_DATA]);
    pIndexes = (GLushort *)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_READ_ONLY);
    fwrite(pIndexes, sizeof(GLushort) * nNumIndexes, 1, pFile);
    glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
    pIndexes = NULL;
    
    // Vertex positions
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[VERTEX_DATA]);
    pVerts = (M3DVector3f *)glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
    fwrite(pVerts, sizeof(M3DVector3f) * nNumVerts, 1, pFile);
    glUnmapBuffer(GL_ARRAY_BUFFER);
    
    // Normals, if we have them
    if(pNorms) { // Look for original flag
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[NORMAL_DATA]);
        pNorms = (M3DVector3f *)glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        fwrite(pNorms, sizeof(M3DVector3f) * nNumVerts, 1, pFile);
        glUnmapBuffer(GL_ARRAY_BUFFER);
        }
        
    // Texture Coordinates, if we have them
    if(pTexCoords) { // Look for original flag
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[TEXTURE_DATA]);
        pTexCoords = (M3DVector2f *)glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        fwrite(pTexCoords, sizeof(M3DVector2f) * nNumVerts, 1, pFile);
        glUnmapBuffer(GL_ARRAY_BUFFER);
        }
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        
  */
    return true;
    }


////////////////////////////////////////////////////////////////////////////////////////////
// Load a mesh into this batch, given the existing and already opened file stream.
// So, You must have verts, but normals and vetexes are optional. You need to know
// ahead of time which are in the file.
bool GLTriangleBatch::LoadMesh(FILE *pFile, bool bNormals, bool bTexCoords)
    {
// In case this is called first (does no harm to call multiple times)
#ifdef QT_IS_AVAILABLE
    initializeOpenGLFunctions();
#endif
    // Create the buffer objects
    glGenBuffers(4, bufferObjects);
    
    // Create the master vertex array object
#if defined ( ANDROID_NDK ) || defined ( __EMSCRIPTEN__ )
	glGenVertexArraysOES(1, &vertexArrayBufferObject);
	glBindVertexArrayOES(vertexArrayBufferObject);
#else
	glGenVertexArrays(1, &vertexArrayBufferObject);
	glBindVertexArray(vertexArrayBufferObject);
#endif

    
    // Read it all in
    fread(&nNumIndexes, sizeof(GLuint), 1, pFile);
    fread(&nNumVerts, sizeof(GLuint), 1, pFile);
    fread(&boundingSphereRadius, sizeof(GLuint), 1, pFile);
    
//    printf("Unique Verts: %d\r\nTriangles: %d\r\n\r\n", nNumVerts, nNumIndexes);
    
    pIndexes = new GLushort[nNumIndexes];
    fread(pIndexes, sizeof(GLushort) * nNumIndexes, 1, pFile);
    
    pVerts = new M3DVector3f[nNumVerts];
    fread(pVerts, sizeof(M3DVector3f) * nNumVerts, 1, pFile);
    
    // Read Normals? If we have them, they occur before the texture coordinates
    if(bNormals) {
        pNorms = new M3DVector3f[nNumVerts];
        if(1 != fread(pNorms, sizeof(M3DVector3f) * nNumVerts, 1, pFile))
            { // dodo head, no normals
            delete [] pNorms;
            pNorms = NULL;
            }
        }
    
    // These are last, so when loading individual meshes from binary, it's okay if we
    // just run out of room. However, for multiple meshes in a single file, we need to
    // know if the mesh has texture coordinates or not. CAD models do not have texture
    // coordinates.
    if(bTexCoords) {
        pTexCoords = new M3DVector2f[nNumVerts];
        if(1 != fread(pTexCoords, sizeof(M3DVector2f) * nNumVerts, 1, pFile))
            {		// Sorry, no texture coordinates
            delete [] pTexCoords;
            pTexCoords = NULL;
            }   
        }
        
    // Vertices
    glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[VERTEX_DATA]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(M3DVector3f) * nNumVerts, pVerts, GL_STATIC_DRAW);
    delete [] pVerts;
    
    // Normals
    if(pNorms) {
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[NORMAL_DATA]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(M3DVector3f) * nNumVerts, pNorms, GL_STATIC_DRAW);
        delete [] pNorms;
        }
        
    // Texture Coordinates
    if(pTexCoords) {
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[TEXTURE_DATA]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(M3DVector2f) * nNumVerts, pTexCoords, GL_STATIC_DRAW);
        delete [] pTexCoords;
        }
    
    // Indexes
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObjects[INDEX_DATA]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort) * nNumIndexes, pIndexes, GL_STATIC_DRAW);
    delete [] pIndexes;
    
    return true;
    }


bool GLTriangleBatch::SaveMesh(const char *szFileName)
	{
	FILE *pFile;
	pFile = fopen(szFileName, "wb");
	if(pFile == NULL)
		return false;
        
    SaveMesh(pFile);

		
	fclose(pFile);
	return true;
	}


bool GLTriangleBatch::LoadMesh(const char *szFileName, bool bNormals, bool bTexCoords)
	{
	FILE *pFile = fopen(szFileName, "rb");
	if(pFile == NULL)
		return false;

    LoadMesh(pFile, bNormals, bTexCoords);


    fclose(pFile);

	return true;
	}
  
