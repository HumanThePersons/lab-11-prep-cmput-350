#ifndef TEXTURE_ATLAS_H
#define TEXTURE_ATLAS_H

#include <cassert>
#include <iostream>
#include <tuple>

#include "vector_util.h"
#include <SFML/Graphics.hpp>

// RectPacker maintains two offsets into an overall bin size:
//  - A line y-offset, representing the current horizontal line on which to place new rectangles
//      horizontally along (the line corresponding to the rectangle tops)
//  - An x-offset into the line, representing the x-coordinate to align the next rectangle left-side
// Furthermore it maintains the maximum texture height encountered along the current line.
// The RectPacker places rectangles left to right along this line until it cannot fit the next
//  rectangle. Once this happens, the line is shifted down to meet the bottom of the tallest texture
//  placed on the line, and the x-offset is reset. This continues until the next rectangle
//  packed cannot be fit on the line within the bin space.
class RectPacker {
public:
    RectPacker(const sf::Vector2f& size) : size(size) {};

    // pack returns { succeeded, rect id }. If not succeeded, id is irrelevant (but ideally should
    // be set to the maximum possible value).
    std::pair<bool, size_t> pack(const sf::Vector2f& rectSize) {
        size_t invalidID = std::numeric_limits<size_t>::max();
        // rectangle is wider than the whole box
        if (rectSize.x > size.x) { return std::pair<bool, size_t>(false, invalidID); }
        
        if (rectSize.y <= (size.y - yOffset)) {
            sf::Vector2f coordinates;

            // if the rectangle doesn't fit in this, it wont fit in the entire rect we are packing
            if (rectSize.x <= (size.x - xOffset)) {
                // Fits on the current line
                coordinates = {xOffset, yOffset};
                xOffset += rectSize.x;
            }
            else {
                // rectabgle will fit in the rect but needs to be put on a new line
                yOffset += maxTexHeight;
                coordinates = {0, yOffset};
                xOffset = rectSize.x;
                maxTexHeight = 0;  // started a new line, it resets
            }

            rectIdVector.push_back(std::pair<sf::Vector2f, sf::Vector2f>(coordinates, rectSize));
            if (rectSize.y > maxTexHeight) {
                // adjust max texture height if needed 
                maxTexHeight = rectSize.y;
            }

            return std::pair<bool, int>(true, rectIdVector.size());
        }

        // Rect doesnt fit
        return std::pair<bool, int>(false, invalidID);  // This should get max id size instead of 9999
    }

    // Returns the coordinates and size of a previously returned rectangle id.
    // Note that in this coordinate system, y points downwards from the origin at the top-left.
    sf::FloatRect getBounds(size_t rectID) const {
        assert(rectID < rectIdVector.size());
        sf::Vector2f coordinates = rectIdVector[rectID].first;
        sf::Vector2f size = rectIdVector[rectID].second;
        return {coordinates, size};
    }

private:
    sf::Vector2f size;
    float yOffset = 0;
    float xOffset = 0;
    float maxTexHeight = 0;
    std::vector<std::pair<sf::Vector2f, sf::Vector2f>> rectIdVector;  // vector of coordinate - size
};

// Returns a new image which pages the given img with *pixels* pixels on each side.
// The colour of new border pixels are taken by sampling the closest valid pixel in the original
// image.
//
// NOTE: a production implementation might also support other edge wrapping behaviors, like cycling
// around to the other side of the texture. See https://learnopengl.com/Getting-started/Textures
// Our implementation corresponds to GL_CLAMP_TO_EDGE
inline sf::Image padImage(const sf::Image& img, uint pixels) {
    sf::Vector2u imgSize = img.getSize();
    sf::Vector2u newSize = {imgSize.x + 2*pixels, imgSize.y + 2*pixels};
    sf::Image newImage(newSize);  // set the padded image size

    // Loop sets the pixels in new image to match old image
    for (int x = 0; x < newSize.x; x++) {
        for (int y = 0; y < newSize.y; y++) {
            // Map to original image coords then clamp to nearest valid pixel
            int sourceX = std::clamp(x - int(pixels), 0, int(imgSize.x) - 1);
            int sourceY = std::clamp(y - int(pixels), 0, int(imgSize.y) - 1);
            sf::Vector2u sourcePixelCoords = {sourceX, sourceY};
            newImage.setPixel({x, y}, img.getPixel(sourcePixelCoords));
        }
    }

    return newImage; 
};

using TextureID = size_t;

class TextureAtlas {
public:
    TextureAtlas(sf::Vector2u binSize, size_t padding)
        : mTexture(binSize), mPacker(sf::Vector2f(mTexture.getSize())), mPadding{padding} {}

    TextureID registerTexture(const std::string& filepath) {
        sf::Image img;
        if (!img.loadFromFile(filepath)) {
            throw std::runtime_error("Cannot load image");
        }

        sf::Image paddedImg = padImage(img, static_cast<uint>(mPadding));

        auto [status, id] = mPacker.pack(sf::Vector2f(paddedImg.getSize()));
        if (!status) {
            // pack returned false
            throw std::runtime_error("Texture atlas is full");
        }

        sf::FloatRect bounds = mPacker.getBounds(id);
        mTexture.update(paddedImg, sf::Vector2u(bounds.position));
        return id; 
    }

    sf::FloatRect getBoundsUV(TextureID id) const {
        sf::FloatRect packed = mPacker.getBounds(id);
        float padding = static_cast<float>(mPadding);
        sf::Vector2f atlasSize(mTexture.getSize());

        // unpadded rect in pixel space
        sf::Vector2f position = packed.position + sf::Vector2f(padding, padding);
        sf::Vector2f size = packed.size = sf::Vector2f(2*padding, 2*padding);

        // convert to UV space - y has to get flipped
        float u = position.x / atlasSize.x;
        float v = 1.0f - ((position.y + size.y) / atlasSize.y);
        return {{u, v}, {size.x / atlasSize.x, size.y / atlasSize.y}};
    }

    sf::Vector2u getSizePixels(TextureID id) const {
        sf::FloatRect packed = mPacker.getBounds(id);
        float padding = static_cast<float>(padding);
        sf::Vector2u pixels = {static_cast<unsigned>(packed.size.x - (2*padding)), 
            static_cast<unsigned>(packed.size.y - (2*padding))};

        return pixels; 
    }

    const sf::Texture& getTexture() const { return mTexture; }

private:
    sf::Texture mTexture;
    RectPacker mPacker;
    size_t mPadding;
};

#endif  // TEXTURE_ATLAS_H
