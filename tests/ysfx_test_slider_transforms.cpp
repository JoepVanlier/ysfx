// Copyright 2024 Joep Vanlier
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//

#include "ysfx.hpp"
#include <catch.hpp>
#include <functional>


void validate_vector(std::function<ysfx_real(ysfx_real)> test_func, std::vector<ysfx_real> ref_value)
{
    ysfx_real x = 0.0;
    for (auto it = ref_value.begin(); it != ref_value.end(); ++it)
    {
        REQUIRE(test_func(x) == Approx(*it).margin(0.001).epsilon(0.005));
        x += 0.05;
    }
}

void validate_inverse(std::function<ysfx_real(ysfx_real)> test_func, std::vector<ysfx_real> ref_value, ysfx_real margin=0.001)
{
    ysfx_real x = 0.0;
    for (auto it = ref_value.begin(); it != ref_value.end(); ++it)
    {
        REQUIRE(test_func(*it) == Approx(x).margin(margin).epsilon(0.005));
        x += 0.05;
    }
}

ysfx_slider_curve_t createCurve(ysfx_real mini, ysfx_real maxi, ysfx_real modifier=0, uint8_t shape=0)
{
    ysfx_slider_curve_t curve{};
    curve.min = mini;
    curve.max = maxi;
    curve.modifier = modifier;
    curve.shape = shape;
    
    return curve;
}

TEST_CASE("slider transforms", "[basic]")
{
    SECTION("API")
    {
        std::vector<ysfx_real> sqrc{20, 136.26, 356.23, 679.91, 1107.31, 1638.4, 2273.21, 3011.73, 3853.96, 4799.89, 5849.54, 7002.89, 8259.96, 9620.73, 11085.21, 12653.4, 14325.31, 16100.91, 17980.23, 19963.26, 22050};
        ysfx_slider_curve_t curve = createCurve(20.0, 22050.0, 2.0, 2);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_normalized_to_ysfx_value(value, &curve); }, sqrc);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_ysfx_value_to_normalized(value, &curve); }, sqrc);
        
        std::vector<ysfx_real> log1{20, 28.39, 40.3, 57.2, 81.19, 115.25, 163.59, 232.2, 329.6, 467.84, 664.08, 942.62, 1338.0, 1899.2, 2695.85, 3826.61, 5431.66, 7709.95, 10943.87, 15534.23, 22050};
        curve = createCurve(20, 22050, 0, 1);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_normalized_to_ysfx_value(value, &curve); }, log1);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_ysfx_value_to_normalized(value, &curve); }, log1);

        curve = createCurve(0, 4, 0, 0);
        curve = createCurve(0, 4, 0, 0);
        std::vector<ysfx_real> lin{0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_normalized_to_ysfx_value(value, &curve); }, lin);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_ysfx_value_to_normalized(value, &curve); }, lin);
    }

    SECTION("SQR")
    {
        ysfx_slider_curve_t curve = createCurve(20.0, 22050.0, 2.0);
        std::vector<ysfx_real> sqrc{20, 136.26, 356.23, 679.91, 1107.31, 1638.4, 2273.21, 3011.73, 3853.96, 4799.89, 5849.54, 7002.89, 8259.96, 9620.73, 11085.21, 12653.4, 14325.31, 16100.91, 17980.23, 19963.26, 22050};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr_raw(value, &curve); }, sqrc);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr_raw(value, &curve); }, sqrc);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, sqrc);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr(value, &curve); }, sqrc);
        
        curve = createCurve(20, 22050, 3);
        std::vector<ysfx_real> sqr3{20, 63.08, 144.47, 276.34, 470.88, 740.29, 1096.73, 1552.41, 2119.49, 2810.18, 3636.64, 4611.07, 5745.66, 7052.58, 8544.02, 10232.17, 12129.22, 14247.34, 16598.72, 19195.54, 22050};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr_raw(value, &curve); }, sqr3);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr_raw(value, &curve); }, sqr3);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, sqr3);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr(value, &curve); }, sqr3);
        
        curve = createCurve(-100, 1500, 2);
        std::vector<ysfx_real> sqr2neg_raw{-100, -81.0, -64.0, -49.0, -36.0, -25, -16.0, -9.0, -4.0, -1.0, 0.0, 15.0, 60.0, 135.0, 240.0, 375.0, 540.0, 735, 960, 1215.0, 1500};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr_raw(value, &curve); }, sqr2neg_raw);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr_raw(value, &curve); }, sqr2neg_raw);
        std::vector<ysfx_real> sqr2neg{-100.0000, -57.2100, -26.2900, -7.2400, -0.064532, 4.7600, 21.3300, 49.7800, 90.1000, 142.2900, 206.3500, 282.2900, 370.1000, 469.7800, 581.3300, 704.7600, 840.0600, 987.2400, 1146.2900, 1317.2100, 1500.0000};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, sqr2neg);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr(value, &curve); }, sqr2neg);
        
        curve = createCurve(500, 1000, 10);
        std::vector<ysfx_real> sqr10{500, 518.24, 537.07, 556.51, 576.59, 597.32, 618.71, 640.8, 663.59, 687.1, 711.37, 736.4, 762.22, 788.85, 816.32, 844.65, 873.86, 903.97, 935.02, 967.02, 1000};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr_raw(value, &curve); }, sqr10);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr_raw(value, &curve); }, sqr10);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, sqr10);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr(value, &curve); }, sqr10);
        
        curve = createCurve(-1000, -500, 5);
        std::vector<ysfx_real> fullneg{-1000, -968.05, -936.93, -906.61, -877.08, -848.33, -820.33, -793.08, -766.56, -740.75, -715.64, -691.22, -667.47, -644.38, -621.93, -600.11, -578.9, -558.31, -538.3, -518.87, -500};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr_raw(value, &curve); }, fullneg);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr_raw(value, &curve); }, fullneg);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, fullneg);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr(value, &curve); }, fullneg);

        curve = createCurve(-1000, 500, 1);
        std::vector<ysfx_real> hmm_raw{-1000, -900, -800, -700, -600, -500, -400, -300, -200, -100, 0, 50, 100, 150, 200, 250, 300, 350, 400, 450, 500};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr_raw(value, &curve); }, hmm_raw);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr_raw(value, &curve); }, hmm_raw);
        std::vector<ysfx_real> hmm{-1000.0000, -925.0000, -850.0000, -775.0000, -700.0000, -625.0000, -550.0000, -475.0000, -400.0000, -325.0000, -250.0000, -175.0000, -100.0000, -25.0000, 50.0000, 125.0000, 200.0000, 275.0000, 350.0000, 425.0000, 500.0000};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, hmm);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr(value, &curve); }, hmm);

        curve = createCurve(-2, -1, 5);
        std::vector<ysfx_real> rev{-2.0, -1.94, -1.87, -1.81, -1.75, -1.7, -1.64, -1.59, -1.53, -1.48, -1.43, -1.38, -1.33, -1.29, -1.24, -1.2, -1.16, -1.12, -1.08, -1.04, -1.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr_raw(value, &curve); }, rev);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr_raw(value, &curve); }, rev, 0.007);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, rev);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr(value, &curve); }, rev, 0.007);
       
        curve = createCurve(-1000, 500, 2);
        std::vector<ysfx_real> lastmod_raw{-1000.0, -810.0, -640.0, -490.0, -360.0, -250.0, -160.0, -90.0, -40.0, -10.0, 0.0, 5.0, 20.0, 45.0, 80.0, 125.0, 180.0, 245.0, 320.0, 405.0, 500.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr_raw(value, &curve); }, lastmod_raw);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr_raw(value, &curve); }, lastmod_raw);
        std::vector<ysfx_real> lastmod{-1000.0000, -836.5700, -687.7200, -553.4400, -433.7300, -328.5800, -238.0200, -162.0200, -100.5900, -53.7300, -21.4500, -3.7300, 0.5900, 12.0200, 38.0200, 78.5800, 133.7300, 203.4400, 287.7200, 386.5700, 500.0000};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, lastmod);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_sqr(value, &curve); }, lastmod);

        curve = createCurve(-5, 0, 2);
        std::vector<ysfx_real> ok{-5.0, -4.512, -4.05, -3.612, -3.2, -2.813, -2.45, -2.112, -1.8, -1.512, -1.25, -1.012, -0.8, -0.612, -0.45, -0.313, -0.2, -0.112, -0.05, -0.0125, 0.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_sqr(value, &curve); }, ok);
    }

    SECTION("LOG")
    {
        ysfx_slider_curve_t curve = createCurve(20, 22050, 0);
        std::vector<ysfx_real> log1{20, 28.39, 40.3, 57.2, 81.19, 115.25, 163.59, 232.2, 329.6, 467.84, 664.08, 942.62, 1338.0, 1899.2, 2695.85, 3826.61, 5431.66, 7709.95, 10943.87, 15534.23, 22050};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, log1);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_log(value, &curve); }, log1);

        curve = createCurve(20, 22050, 100);
        std::vector<ysfx_real> log2{20, 20.22, 20.61, 21.28, 22.47, 24.55, 28.21, 34.61, 45.83, 65.5, 100.0, 160.48, 266.51, 452.4, 778.31, 1349.7, 2351.46, 4107.76, 7186.94, 12585.38, 22050};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, log2);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_log(value, &curve); }, log2);
        
        curve = createCurve(-500, 1000, 200);
        std::vector<ysfx_real> log5{-500, -434.13, -367.38, -299.72, -231.16, -161.68, -91.26, -19.9, 52.42, 125.72, 200.0, 275.28, 351.57, 428.89, 507.24, 586.65, 667.13, 748.69, 831.34, 915.11, 1000};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, log5);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_log(value, &curve); }, log5);

        curve = createCurve(20, 22050, 5000);
        std::vector<ysfx_real> barf{20, 289.1, 593.44, 937.64, 1326.91, 1767.17, 2265.09, 2828.22, 3465.09, 4185.38, 5000, 5921.31, 6963.27, 8141.7, 9474.47, 10981.78, 12686.49, 14614.47, 16794.95, 19260.99, 22050};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, barf);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_log(value, &curve); }, barf);

        curve = createCurve(-1000, 1000, 0);
        std::vector<ysfx_real> last{-1000, -900, -800, -700, -600, -500, -400, -300, -200, -100, 0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, last);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_log(value, &curve); }, last);
        
        curve = createCurve(-1000, -10, -100);
        std::vector<ysfx_real> another{-1000, -794.33, -630.96, -501.19, -398.11, -316.23, -251.19, -199.53, -158.49, -125.89, -100.0, -79.43, -63.1, -50.12, -39.81, -31.62, -25.12, -19.95, -15.85, -12.59, -10};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, another);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_log(value, &curve); }, another);
    }

    SECTION("LIN")
    {
        ysfx_slider_curve_t curve = createCurve(0, 4);
        std::vector<ysfx_real> lin{0.0, 0.2, 0.4, 0.6, 0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, lin);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear_raw(value, &curve); }, lin);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, lin);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear(value, &curve); }, lin);

        curve = createCurve(0, -4);
        std::vector<ysfx_real> lin2{0.0, -0.2, -0.4, -0.6, -0.8, -1.0, -1.2, -1.4, -1.6, -1.8, -2.0, -2.2, -2.4, -2.6, -2.8, -3.0, -3.2, -3.4, -3.6, -3.8, -4.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, lin2);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear_raw(value, &curve); }, lin2);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, lin2);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear(value, &curve); }, lin2);

        curve = createCurve(-4, 0);
        std::vector<ysfx_real> lin3{-4.0, -3.8, -3.6, -3.4, -3.2, -3.0, -2.8, -2.6, -2.4, -2.2, -2.0, -1.8, -1.6, -1.4, -1.2, -1.0, -0.8, -0.6, -0.4, -0.2, 0.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, lin3);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear_raw(value, &curve); }, lin3);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, lin3);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear(value, &curve); }, lin3);

        ysfx_real tiny = 0.0000000000000000000000001;
        curve = createCurve(-4, 10 * tiny);
        std::vector<ysfx_real> lin4_raw{-4.0, -3.6, -3.2, -2.8, -2.4, -2.0, -1.6, -1.2, -0.8, -0.4, 0.0, tiny, 2 * tiny, 3 * tiny, 4 * tiny, 5 * tiny, 6 * tiny, 7 * tiny, 8 * tiny, 9 * tiny};
        validate_vector([curve, tiny](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, lin4_raw);
        validate_inverse([curve, tiny](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear_raw(value, &curve); }, lin4_raw);
        std::vector<ysfx_real> lin4{-4.0000, -3.8000, -3.6000, -3.4000, -3.2000, -3.0000, -2.8000, -2.6000, -2.4000, -2.2000, -2.0000, -1.8000, -1.6000, -1.4000, -1.2000, -1.0000, -0.8000, -0.6000, -0.4000, -0.2000, 0.0000};
        validate_vector([curve, tiny](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, lin4);
        validate_inverse([curve, tiny](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear(value, &curve); }, lin4);

        curve = createCurve(-3, 1);
        std::vector<ysfx_real> lin5_raw{-3.0, -2.7, -2.4, -2.1, -1.8, -1.5, -1.2, -0.9, -0.6, -0.3, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, lin5_raw);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear_raw(value, &curve); }, lin5_raw);
        std::vector<ysfx_real> lin5{-3.0000, -2.8000, -2.6000, -2.4000, -2.2000, -2.0000, -1.8000, -1.6000, -1.4000, -1.2000, -1.0000, -0.8000, -0.6000, -0.4000, -0.2000, 0.0000, 0.2000, 0.4000, 0.6000, 0.8000, 1.0000};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, lin5);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear(value, &curve); }, lin5);

        curve = createCurve(-4, -0);
        std::vector<ysfx_real> lin6{-4.0, -3.8, -3.6, -3.4, -3.2, -3.0, -2.8, -2.6, -2.4, -2.2, -2.0, -1.8, -1.6, -1.4, -1.2, -1.0, -0.8, -0.6, -0.4, -0.2, 0.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, lin6);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear_raw(value, &curve); }, lin6);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, lin6);
        validate_inverse([curve](float value) -> ysfx_real { return ysfx_slider_scale_to_normalized_linear(value, &curve); }, lin6);
    }

    SECTION("Invalid")
    {
        ysfx_slider_curve_t curve = createCurve(0, 0);
        std::vector<ysfx_real> bad_range{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, bad_range);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, bad_range);

        curve = createCurve(1, 1);
        std::vector<ysfx_real> bad_range2{1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, bad_range2);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, bad_range2);

        curve = createCurve(-1, -1);
        std::vector<ysfx_real> bad_range3{-1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0, -1.0};
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear_raw(value, &curve); }, bad_range3);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_linear(value, &curve); }, bad_range3);

        curve = createCurve(0, 0, 0);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, bad_range);
        curve = createCurve(1, 1, 1);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, bad_range2);
        curve = createCurve(-1, -1, -1);
        validate_vector([curve](float value) -> ysfx_real { return ysfx_slider_scale_from_normalized_log(value, &curve); }, bad_range3);
    }
}
