#include "BasicScene.h"
#include <utility>
#include "ObjLoader.h"
#include "IglMeshLoader.h"
#include <igl/circulation.h>
#include <igl/collapse_edge.h>
#include <igl/edge_flaps.h>
#include <igl/decimate.h>
#include <igl/shortest_edge_and_midpoint.h>
#include <igl/parallel_for.h>
#include <igl/read_triangle_mesh.h>
#include <igl/opengl/glfw/Viewer.h>
#include <Eigen/Core>
#include <iostream>
#include <set>
#include "../engine/Mesh.h"
//#include "AutoMorphingModel.h"

using namespace cg3d;

enum keyPress { UP, DOWN, UNPRESSED };
keyPress lastKey = UNPRESSED;
int lastState = 0;

std::vector<MeshData> BasicScene::createDecimatedMesh(std::string filename)
{
    auto currentMesh{ IglLoader::MeshFromFiles("cyl_igl", filename) };
    Eigen::MatrixXd V, OV;
    Eigen::MatrixXi F, OF;

    igl::read_triangle_mesh(filename, OV, OF);

    // Prepare array-based edge data structures and priority queue
    Eigen::VectorXi EMAP;
    Eigen::MatrixXi E, EF, EI;
    igl::min_heap< std::tuple<double, int, int> > Q;
    Eigen::VectorXi EQ;
    // If an edge were collapsed, we'd collapse it to these points:
    Eigen::MatrixXd C;
    int num_collapsed;

    // Function to reset original mesh and data structures
    const auto& reset = [&]()
    {
        F = OF;
        V = OV;
        igl::edge_flaps(F, E, EMAP, EF, EI);
        C.resize(E.rows(), V.cols());
        Eigen::VectorXd costs(E.rows());
        Q = {};
        EQ = Eigen::VectorXi::Zero(E.rows());
        {
            Eigen::VectorXd costs(E.rows());
            igl::parallel_for(E.rows(), [&](const int e)
                {
                    double cost = e;
                    Eigen::RowVectorXd p(1, 3);
                    igl::shortest_edge_and_midpoint(e, V, F, E, EMAP, EF, EI, cost, p);
                    C.row(e) = p;
                    costs(e) = cost;
                }, 10000);
            for (int e = 0; e < E.rows(); e++)
            {
                Q.emplace(costs(e), e, 0);
            }
        }
        num_collapsed = 0;
    };

    reset();

    std::vector<cg3d::MeshData> meshDataVector;
    meshDataVector.push_back(currentMesh->data[0]);
    for (int i = 0; i < 6; i++)
    {
        std::cout << "this is a test\n";
        const int max_iter = std::ceil(0.01 * Q.size());
        for (int j = 0; j < max_iter; j++)
        {
            //std::cout << "iteration number " << j << std::endl;
            //std::cout << "V: \n" << V << std::endl;
            //std::cout << "F: \n" << F << std::endl;
            //std::cout << "E: \n" << E << std::endl;
            //std::cout << "EMAP: \n" << EMAP << std::endl;
            //std::cout << "EF: \n" << EF << std::endl;
            //std::cout << "EI: \n" << EI << std::endl;
            ////std::cout << "Q: \n" << Q << std::endl;
            //std::cout << "EQ: \n" << EQ << std::endl;
            //std::cout << "C: \n" << C << std::endl;

            if (!igl::collapse_edge(igl::shortest_edge_and_midpoint, V, F, E, EMAP, EF, EI, Q, EQ, C))
            {
                break;
            }
        }
        std::cout << "this is a test\n";
        cg3d::Mesh temp("new mesh", V, F, currentMesh->data[0].vertexNormals, currentMesh->data[0].textureCoords);
        meshDataVector.push_back(temp.data[0]);
    }

    /*auto snakeMesh{ ObjLoader::MeshFromObjFiles<std::string>("snakeMesh", "data/sphere.obj", "data/camel_b.obj", "data/armadillo.obj", "data/arm.obj", "data/monkey3.obj") };
    return snakeMesh->data;*/
    std::cout << "Decimated mesh size is: " << meshDataVector.size() << std::endl;
    return meshDataVector;
}

void BasicScene::Init(float fov, int width, int height, float near, float far)
{
    using namespace std;
    using namespace Eigen;
    using namespace igl;
    camera = Camera::Create( "camera", fov, float(width) / height, near, far);
    
    AddChild(root = Movable::Create("root")); // a common (invisible) parent object for all the shapes
    auto daylight{std::make_shared<Material>("daylight", "shaders/cubemapShader")}; 
    daylight->AddTexture(0, "textures/cubemaps/Daylight Box_", 3);
    auto background{Model::Create("background", Mesh::Cube(), daylight)};
    AddChild(background);
    background->Scale(120, Axis::XYZ);
    background->SetPickable(false);
    background->SetStatic();

    auto program = std::make_shared<Program>("shaders/basicShader");
    auto material{ std::make_shared<Material>("material", program)}; // empty material
    material->AddTexture(0, "textures/box0.bmp", 2);

    std::shared_ptr<cg3d::Mesh> camelMesh(new cg3d::Mesh(std::string("camelWithDecimations"), createDecimatedMesh("data/lion.off")));
    camel = Model::Create("camel", camelMesh, material);

    auto morphFunc = [](Model* model, cg3d::Visitor* visitor) {
        //std::cout << "Current state is: " << lastState << std::endl; //for debugging state
        if (lastKey == UP && lastState > 0)
        {
            lastKey = UNPRESSED;
            return --lastState;
        }
            
        if (lastKey == DOWN && lastState < 6)
        {
            lastKey = UNPRESSED;
            return ++lastState;
        }

        lastKey = UNPRESSED;
        return lastState;
    };

    autoCamel = cg3d::AutoMorphingModel::Create(*camel, morphFunc);
    autoCamel->Translate({ 3,0,0 });
    autoCamel->Scale(3.0f);
    autoCamel->showWireframe = true;
    root->AddChild(autoCamel);
    camera->Translate(20, Axis::Z);
}

void BasicScene::Update(const Program& program, const Eigen::Matrix4f& proj, const Eigen::Matrix4f& view, const Eigen::Matrix4f& model)
{
    Scene::Update(program, proj, view, model);
    program.SetUniform4f("lightColor", 1.0f, 1.0f, 1.0f, 0.5f);
    program.SetUniform4f("Kai", 1.0f, 1.0f, 1.0f, 1.0f);
    autoCamel->Rotate(0.001f, Axis::Y);
}

void BasicScene::KeyCallback(Viewport* viewport, int x, int y, int key, int scancode, int action, int mods)
{
    auto system = camera->GetRotation().transpose();

    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        switch (key) // NOLINT(hicpp-multiway-paths-covered)
        {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_UP:
            //camera->RotateInSystem(system, 0.1f, Axis::X);
            lastKey = UP;
            break;
        case GLFW_KEY_DOWN:
            //camera->RotateInSystem(system, -0.1f, Axis::X);
            lastKey = DOWN;
            break;
        case GLFW_KEY_LEFT:
            camera->RotateInSystem(system, 0.1f, Axis::Y);
            break;
        case GLFW_KEY_RIGHT:
            camera->RotateInSystem(system, -0.1f, Axis::Y);
            break;
        case GLFW_KEY_W:
            camera->TranslateInSystem(system, { 0, 0.05f, 0 });
            break;
        case GLFW_KEY_S:
            camera->TranslateInSystem(system, { 0, -0.05f, 0 });
            break;
        case GLFW_KEY_A:
            camera->TranslateInSystem(system, { -0.05f, 0, 0 });
            break;
        case GLFW_KEY_D:
            camera->TranslateInSystem(system, { 0.05f, 0, 0 });
            break;
        case GLFW_KEY_B:
            camera->TranslateInSystem(system, { 0, 0, 0.05f });
            break;
        case GLFW_KEY_F:
            camera->TranslateInSystem(system, { 0, 0, -0.05f });
            break;
        default:
            lastKey = UNPRESSED;
            break;
        }
    }
}