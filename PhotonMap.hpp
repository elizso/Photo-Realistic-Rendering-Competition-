#pragma once
#include "Vector.hpp"
#include <vector> 
#include <algorithm> 

struct Photon {
    Vector3f position;
    Vector3f power;
    Vector3f direction;
    float lambda;
}; 

struct KDNode {
    Photon photon;
    int left = -1;
    int right = -1;
    int axis = 0;
};

class PhotonMap {
    public: 
        std::vector<Photon> photons;
        std::vector<int> tree;
        bool built = false;
        int n = 0;
        
        void store (const Photon& p){
            photons.push_back(p);
        }

        void build() {
            n = (int) photons.size();
            if (n == 0) return;
            tree.resize(n);
            for (int i = 0; i < n; i++) tree[i] = i;
            buildKD(0, n, 0);
            built = true;
            printf("Photon map built with %d photons.\n", n);
        }

        void query(const Vector3f& pos, float max_dist, int k, std::vector<const Photon*>& result) const {
            result.clear();
            if (!built || n == 0) return;
            queryKD(0, n, pos, max_dist * max_dist, result);
        }
    private: 
        void buildKD(int start, int end, int depth) {
            if (end - start <= 0) return;
            int axis = depth % 3;
            int mid = (start + end) / 2;
            std::nth_element(
                tree.begin() + start, 
                tree.begin() + mid, 
                tree.begin() + end, 
                [&](int i, int j) {
                    return photons[i].position[axis] < photons[j].position[axis];
                }
            );
            buildKD(start, mid, depth + 1); 
            buildKD(mid+1, end, depth + 1);
        }

        void queryKD(int start, int end, const Vector3f& pos, float max_dist2, 
                    std::vector<const Photon*>& result) const {
            if (start >= end) return;
            int mid        = (start + end) / 2;
            int axis       = __builtin_ctz(end - start) % 3; // approximate axis
            const Photon& ph = photons[tree[mid]];

            // Check this photon
            Vector3f d = ph.position - pos;
            float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
            if (dist2 <= max_dist2)
                result.push_back(&ph);

            // Recurse into children
            float axisDist = pos[axis] - ph.position[axis];
            if (axisDist <= 0) {
                queryKD(start, mid, pos, max_dist2, result);
                if (axisDist * axisDist <= max_dist2)
                    queryKD(mid + 1, end, pos, max_dist2, result);
            } else {
                queryKD(mid + 1, end, pos, max_dist2, result);
                if (axisDist * axisDist <= max_dist2)
                    queryKD(start, mid, pos, max_dist2, result);
            }
        }
};