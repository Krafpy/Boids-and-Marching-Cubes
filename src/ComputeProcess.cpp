#include "ComputeProcess.h"

#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>


ComputeProcess::ComputeProcess()
{
    if(!gotCapabilities){
        getWorkGroupsCapabilities();
        printWorkGroupsCapabilities();
    }
}

GLuint ComputeProcess::createComputeProgram(std::string sourcefile)
{
    GLuint csProgramID;
    GLuint shaderID;

    char *source = loadShaderSource(sourcefile);

    shaderID = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shaderID, 1, &source, NULL);
    glCompileShader(shaderID);

    delete[] source;

    GLint result = GL_FALSE;
    int InfoLogLength = 1024;
    char shaderErrorMessage[1024] = {0};

    glGetShaderiv(shaderID, GL_COMPILE_STATUS, &result);

    glGetShaderInfoLog(shaderID, InfoLogLength, NULL, shaderErrorMessage);
    if (result == GL_FALSE)
      std::cout << "\nSHADER ERROR:\n" << shaderErrorMessage << "\n";

    csProgramID = glCreateProgram();
    glAttachShader(csProgramID, shaderID);
    glLinkProgram(csProgramID);
    glDeleteShader(shaderID);

    return csProgramID;
}

char* ComputeProcess::loadShaderSource(std::string filename)
{
    std::ifstream file(filename);
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    char* source = new char[content.length()+1];
    strcpy(source, content.c_str());

    return source;
}

void ComputeProcess::useProgram(ComputeProgram& cprogram)
{
    currentProgram = cprogram;
    glUseProgram(currentProgram.id);
}

void ComputeProcess::runComputeShader(int num_workgroup_x, int num_workgroup_y, int num_workgroup_z)
{
    glDispatchCompute(num_workgroup_x, num_workgroup_y, num_workgroup_z);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void ComputeProcess::runComputeShader(int num_workgroup_x, int num_workgroup_y, int num_workgroup_z, int workgroup_size_x, int workgroup_size_y, int workgroup_size_z)
{
    glDispatchComputeGroupSizeARB(num_workgroup_x, num_workgroup_y, num_workgroup_z, workgroup_size_x, workgroup_size_y, workgroup_size_z);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void ComputeProcess::runComputeShader(ComputeProgram& cprogram)
{
    Volume& workgroups = cprogram.dispatchParams.numWorkgroups;
    Volume& localSize = cprogram.dispatchParams.workgroupSize;

    glDispatchComputeGroupSizeARB(workgroups.x, workgroups.y, workgroups.z, localSize.x, localSize.y, localSize.z);
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
}

void ComputeProcess::runComputeShader()
{
    runComputeShader(currentProgram);
}

void ComputeProcess::startDurationRecording()
{
    glGenQueries(1, &durationQuery);
    glBeginQuery(GL_TIME_ELAPSED, durationQuery);
}

float ComputeProcess::endDurationRecording()
{
    unsigned int durationNano;

    glEndQuery(GL_TIME_ELAPSED);
    glGetQueryObjectuiv(durationQuery, GL_QUERY_RESULT, &durationNano);
    glDeleteQueries(1, &durationQuery);

    return (float)durationNano/(float)1e6; // convert from ns to ms;
}

ComputeProcess::DispatchParams ComputeProcess::calculateOptimalDisptachSpace(int numInstancesX, int numInstancesY, int numInstancesZ)
{
    // Calculate the number of workgroups and local invocations for optimized performances :
    // maximize the local size while minimizing the number of workgroups

    int rangeX = std::min(numInstancesX, workGroupsCapabilities[3]);
    int rangeY = std::min(numInstancesY, workGroupsCapabilities[4]);
    int rangeZ = std::min(numInstancesZ, workGroupsCapabilities[5]);

    int record = 0;

    std::vector<Volume> localSizeOptions;
    unsigned int startIndex = 0;

    for(int x = 1; x <= rangeX; x++){
        if(numInstancesX % x != 0)
            continue;
        for(int y = 1; y <= rangeY; y++){
            if(numInstancesY % y != 0)
                continue;
            for(int z = 1; z <= rangeZ; z++){
                if(numInstancesZ % z != 0)
                    continue;
                Volume v(x, y, z);
                if(v.count < workGroupsCapabilities[6]){
                    if(v.count >= record){
                        if(v.count > record)
                            startIndex = localSizeOptions.size();
                        record = v.count;
                        localSizeOptions.push_back(v);
                    }
                }
            }
        }
    }

    // Arbitrarily we choose the repartition of workgroups that corresponds to
    // the limits of number workgroups along each axes, i.e x >= y >= z

    DispatchParams bestParams;

    // Use unsigned ints for a bigger value range
    // Prefer using longs with a 64bit compiler

    unsigned int min = 0;

    unsigned int maxWorkgroupsX = (unsigned int)workGroupsCapabilities[0];
    unsigned int maxWorkgroupsY = (unsigned int)workGroupsCapabilities[1];
    unsigned int maxWorkgroupsZ = (unsigned int)workGroupsCapabilities[2];

    for(unsigned int i = startIndex; i < localSizeOptions.size(); i++){
        Volume localSize = localSizeOptions[i];
        Volume workgroup(numInstancesX / localSize.x, numInstancesY / localSize.y, numInstancesZ / localSize.z);

        unsigned int numWorkgroupsX = (unsigned int)workgroup.x;
        unsigned int numWorkgroupsY = (unsigned int)workgroup.y;
        unsigned int numWorkgroupsZ = (unsigned int)workgroup.z;

        unsigned int d = maxWorkgroupsX + maxWorkgroupsY + maxWorkgroupsZ - numWorkgroupsX - numWorkgroupsY - numWorkgroupsZ;

        if(d < min || i == startIndex){
            min = d;
            bestParams = DispatchParams(workgroup, localSize);
        }
    }

    printf("%d %d %d workgroups of size %d %d %d\n",
           bestParams.numWorkgroups.x, bestParams.numWorkgroups.y, bestParams.numWorkgroups.z,
           bestParams.workgroupSize.x, bestParams.workgroupSize.y, bestParams.workgroupSize.z);

    return bestParams;
}

int ComputeProcess::workGroupsCapabilities[7];
bool ComputeProcess::gotCapabilities = false;

void ComputeProcess::getWorkGroupsCapabilities()
{
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workGroupsCapabilities[0]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &workGroupsCapabilities[1]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &workGroupsCapabilities[2]);

  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &workGroupsCapabilities[3]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &workGroupsCapabilities[4]);
  glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &workGroupsCapabilities[5]);

  glGetIntegerv (GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &workGroupsCapabilities[6]);

  gotCapabilities = true;
}

void ComputeProcess::printWorkGroupsCapabilities()
{
    printf("Max number of workgroups:\nx: %d\ny: %d\nz: %d\n", workGroupsCapabilities[0], workGroupsCapabilities[1], workGroupsCapabilities[2]);
    printf("Max number of local invocations:\nx: %d\ny: %d\nz: %d\n", workGroupsCapabilities[3], workGroupsCapabilities[4], workGroupsCapabilities[5]);
    printf("Max total number of local invocations: %d\n\n", workGroupsCapabilities[6]);
}

ComputeProcess::Volume::Volume()
{

}

ComputeProcess::Volume::Volume(int _x, int _y, int _z) : x(_x), y(_y), z(_z), count(_x * _y * _z)
{

}

ComputeProcess::DispatchParams::DispatchParams()
{

}

ComputeProcess::DispatchParams::DispatchParams(Volume _numWorkgroups, Volume _workgroupSize) : numWorkgroups(_numWorkgroups), workgroupSize(_workgroupSize)
{

}

ComputeProcess::ComputeProgram::ComputeProgram()
{

}

ComputeProcess::ComputeProgram::ComputeProgram(std::string sourcefile, ComputeProcess::DispatchParams params) : dispatchParams(params)
{
    id = createComputeProgram(sourcefile);
}
