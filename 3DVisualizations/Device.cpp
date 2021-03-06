#include "stdafx.h"
#include "Device.h"

#include <algorithm>
#include <limits>

constexpr double epsilon = 0.00001;

Device::Device()
    : deviceWidth(0), deviceHeight(0), depthBufferSize(0),
      backBuffer(BackBuffer()), depthBuffer(std::vector<double>()) {}

Device::Device(int pixelWidth, int pixelHeight)
    : deviceWidth(pixelWidth), deviceHeight(pixelHeight), depthBufferSize(pixelWidth * pixelHeight) {
    backBuffer = BackBuffer(pixelWidth, pixelHeight);
    depthBuffer.resize(depthBufferSize);
}

void Device::Clear(Color4 fillColor) {
    // Buffer data is in 4 byte-per-pixel format, iterates from 0 to end of buffer
    for (auto index = 0; index < (backBuffer.scanLineSize * backBuffer.height); index += 4) {
        // BGRA is the color system used by Windows.
        backBuffer[index] = (char)(fillColor.Blue * 255);
        backBuffer[index + 1] = (char)(fillColor.Green * 255);
        backBuffer[index + 2] = (char)(fillColor.Red * 255);
        backBuffer[index + 3] = (char)(fillColor.Alpha * 255);
    }

    // Clearing Depth Buffer
    for (auto index = 0; index < depthBufferSize; index++) {
        depthBuffer[index] = std::numeric_limits<double>::max();
    }
}

BackBuffer Device::GetBuffer() const {
    return backBuffer;
}

void Device::RenderSurface(const Camera& camera, const Mesh& mesh, const Quaternion& rotation) {
    Matrix viewMatrix = Matrix::LookAtLH(camera.Position, camera.Target, Vector3(0.0, 0.0, 1.0));
    Matrix projectionMatrix = Matrix::PerspectiveFovLH(0.78f,
        (double)deviceWidth / deviceHeight,
        0.01f, 1.0f);

    Matrix transformMatrix = viewMatrix * projectionMatrix;

    auto faceIndex = 0;
    for (const auto& face : mesh.Faces) {
        // Get each vertex for this face
        const auto& vertexA = mesh.Vertices[face.A];
        const auto& vertexB = mesh.Vertices[face.B];
        const auto& vertexC = mesh.Vertices[face.C];

        // Transform to get the pixel
        auto pixelA = Project(vertexA, rotation, transformMatrix);
        auto pixelB = Project(vertexB, rotation, transformMatrix);
        auto pixelC = Project(vertexC, rotation, transformMatrix);

        Vector3 normal = rotation.Rotate(face.normal);
        Vector3 position = rotation.Rotate(face.position);
        Vector3 light = Vector3::Normalize(Vector3::Subtract(camera.Light, position));

        // Rasterize face as a triangles
        RasterizeTriangle(pixelA, pixelB, pixelC, Color4::Shade(light, normal, face.color, 0.25));
        faceIndex++;
    }
}

void Device::RenderWireframe(const Camera& camera, const Mesh& mesh, const Quaternion& rotation, const Color4& color, int thickness) {
    Matrix viewMatrix = Matrix::LookAtLH(camera.Position, camera.Target, Vector3(0.0, 0.0, 1.0));
    Matrix projectionMatrix = Matrix::PerspectiveFovLH(0.78f,
        (double)deviceWidth / deviceHeight,
        0.01f, 1.0f);

    Matrix transformMatrix = viewMatrix * projectionMatrix;

    for (const auto& edge : mesh.Edges) {
        const auto& vertexA = mesh.Vertices[edge.first];
        const auto& vertexB = mesh.Vertices[edge.second];

        // Transform to get the pixel
        auto pixelA = Project(vertexA, rotation, transformMatrix);
        auto pixelB = Project(vertexB, rotation, transformMatrix);

        DrawLine(pixelA, pixelB, color, thickness);
    }
}

Vector3 Device::Project(const Vector3& coord, const Quaternion& rotation, const Matrix& transMat) const {
    // Rotate
    Vector3 point = rotation.Rotate(coord);

    // Transforming the coordinates
    point = Vector3::TransformCoordinate(point, transMat);

    // The transformed coordinates will be based on coordinate system
    // starting on the center of the screen. But drawing on screen normally starts
    // from top left. We then need to transform them again to have x:0, y:0 on top left.
    // Includes Z pos for the Z-Buffer, un-transformed
    double x = point.X * deviceWidth + deviceWidth / 2.0f;
    double y = -point.Y * deviceHeight + deviceHeight / 2.0f;
    return (Vector3(x, y, point.Z));
}

void Device::RasterizeTriangle(Vector3 p1, Vector3 p2, Vector3 p3, Color4 color) {
    // Sort points
    if (p1.Y > p2.Y) {
        auto temp = p2;
        p2 = p1;
        p1 = temp;
    }
    if (p2.Y > p3.Y) {
        auto temp = p2;
        p2 = p3;
        p3 = temp;
    }
    if (p1.Y > p2.Y) {
        auto temp = p2;
        p2 = p1;
        p1 = temp;
    }

    if ((abs(p1.Y - p3.Y) < 0.2)) {
        return;
    }

    if ((abs(p1.Y - p2.Y) < epsilon) && p1.X > p2.X) {
        auto temp = p2;
        p2 = p1;
        p1 = temp;
    }

    // Calculate inverse slopes
    double dP1P2, dP1P3;
    bool horizontal = false;

    if (p2.Y - p1.Y > epsilon) {
        dP1P2 = (p2.X - p1.X) / (p2.Y - p1.Y);
    }
    else {
        dP1P2 = 0;
        horizontal = true;
    }

    if (p3.Y - p1.Y > epsilon) {
        dP1P3 = (p3.X - p1.X) / (p3.Y - p1.Y);
    }
    else {
        dP1P3 = 0;
    }

    // Two cases for triangle shape once points are sorted
    if (horizontal || dP1P2 > dP1P3) {
        // Iterate over height of triangle
        for (auto y = (int)p1.Y; y <= (int)p3.Y; y++) {
            // Reverse once second point is reached
            if (y < p2.Y) {
                // Draw line across triangle's width
                ProcessScanLine(y, p1, p3, p1, p2, color);
            }
            else {
                ProcessScanLine(y, p1, p3, p2, p3, color);
            }
        }
    }
    else {
        for (auto y = (int)p1.Y; y <= (int)p3.Y; y++) {
            if (y < p2.Y) {
                ProcessScanLine(y, p1, p2, p1, p3, color);
            }
            else {
                ProcessScanLine(y, p2, p3, p1, p3, color);
            }
        }
    }
}

double Device::Clamp(double value, double min, double max) {
    return std::max(min, std::min(value, max));
}

double Device::Interpolate(double min, double max, double gradient) {
    return min + ((max - min) * gradient);
}

void Device::ProcessScanLine(int y, Vector3 pa, Vector3 pb, Vector3 pc, Vector3 pd, Color4 color) {
    auto gradient1 = (abs(pa.Y - pb.Y) > epsilon) ? (y - pa.Y) / (pb.Y - pa.Y) : 1;
    auto gradient2 = (abs(pc.Y - pd.Y) > epsilon) ? (y - pc.Y) / (pd.Y - pc.Y) : 1;

    int sx = (int)Interpolate(pa.X, pb.X, gradient1);
    int ex = (int)Interpolate(pc.X, pd.X, gradient2);

    double z1 = Interpolate(pa.Z, pb.Z, gradient1);
    double z2 = Interpolate(pc.Z, pd.Z, gradient2);

    // drawing a line from left (sx) to right (ex)
    for (auto x = sx; x < ex; x++) {
        double gradient = (x - sx) / (double)(ex - sx);

        auto z = Interpolate(z1, z2, gradient);
        DrawPoint(Vector3(x, y, z), color);
    }
}

double delta(int x, int y) {
    double x2 = x * x;
    double y2 = y * y;

    return sqrt(x2 + y2);
}

void Device::DrawLine(Vector3 pointA, Vector3 pointB, Color4 color, int width) {
    int x0 = (int)pointA.X;
    int y0 = (int)pointA.Y;
    int x1 = (int)pointB.X;
    int y1 = (int)pointB.Y;
    int x2;
    int y2;

    int wd = (width + 1) / 2;

    auto dx = abs(x1 - x0);
    auto dy = abs(y1 - y0);
    auto sx = (x0 < x1) ? 1 : -1;
    auto sy = (y0 < y1) ? 1 : -1;
    auto err = dx - dy;
    double ed = dx + dy == 0 ? 1 : delta(dx, dy);

    int x = x0;
    int y = y0;

    while (true) {
        double t = delta(x - x0, y - y0) / delta(dx, dy);
        double z = Interpolate(pointA.Z, pointB.Z, t);
        //double brightness = std::min(1.0, std::max(0.0, 1.0 - (abs(err - dx + dy) / ed)));
        DrawPoint(Vector3(x, y, z), color);

        double e2 = err;
        x2 = x;
        if (2 * e2 >= -dx) {
            for (e2 += dy, y2 = y; e2 < ed * wd && (y1 != y2 || dx > dy); e2 += dx) {
                //brightness = std::min(1.0, std::max(0.0, 1.0 - abs(e2) / ed));
                y2 += sy;
                DrawPoint(Vector3(x, y2, z), color);
            }
            if (x == x1) break;
            e2 = err; err -= dy; x += sx;
        }

        if (2 * e2 <= dy) {
            for (e2 = dx - e2; e2 < ed * wd && (x1 != x2 || dx < dy); e2 += dy) {
                //brightness = std::min(1.0, std::max(0.0, 1.0 - abs(e2) / ed));
                x2 += sx;
                DrawPoint(Vector3(x2, y, z), color);
            }
            if (y == y1) break;
            err += dx; y += sy;
        }
    }
}

void Device::DrawPoint(Vector3 point, Color4 color) {
    // Clip to device size
    if (point.X < 0 || point.Y < 0 || point.X >= deviceWidth || point.Y >= deviceHeight) {
        return;
    }

    auto index = (int)point.X + ((int)point.Y * deviceWidth);

    if (point.Z > depthBuffer[index]) {
        return; // Discard
    }

    depthBuffer[index] = point.Z;

    PutPixel((int)point.X, (int)point.Y, color);
}

void Device::PutPixel(int x, int y, Color4 color) {
    // As we have a 1-D Array for our back buffer
    // we need to know the equivalent cell in 1-D based
    // on the 2D coordinates on screen
    auto index = ((x * 4) + y * (backBuffer.scanLineSize));

    backBuffer[index] = (char)(color.Blue * 255) + (char)((1.0 - color.Alpha) * (double)backBuffer[index]);
    backBuffer[index + 1] = (char)(color.Green * 255) + (char)((1.0 - color.Alpha) * (double)backBuffer[index + 1]);
    backBuffer[index + 2] = (char)(color.Red * 255) + (char)((1.0 - color.Alpha) * (double)backBuffer[index + 2]);
    backBuffer[index + 3] = (char)(color.Alpha * 255) + (char)((1.0 - color.Alpha) * (double)backBuffer[index + 3]);
}

int Device::getWidth() {
    return deviceWidth;
}

int Device::getHeight() {
    return deviceHeight;
}
