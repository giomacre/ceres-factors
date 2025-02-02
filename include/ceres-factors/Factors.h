#pragma once

#include <Eigen/Core>
#include <ceres/ceres.h>
#include <SO3.h>
#include <SE3.h>

using namespace Eigen;

// AutoDiff cost function (factor) for the difference between two rotations.
// Weighted by measurement covariance, Q_.
class SO3Factor
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  // store measured relative pose and inverted covariance matrix
  SO3Factor(const Vector4d &q_vec, const Matrix3d &Q)
      : q_(q_vec), Q_inv_(Q.inverse())
  {
  }

  // templated residual definition for both doubles and jets
  // basically a weighted implementation of boxminus using Eigen templated types
  template <typename T>
  bool operator()(const T *_q_hat, T *_res) const
  {
    SO3<T> q_hat(_q_hat);
    Map<Matrix<T, 3, 1>> r(_res);
    r = Q_inv_ * (q_hat - q_.cast<T>());
    return true;
  }

  static ceres::CostFunction *Create(const Vector4d &q_vec, const Matrix3d &Q)
  {
    return new ceres::AutoDiffCostFunction<SO3Factor,
                                           3,
                                           4>(new SO3Factor(q_vec, Q));
  }

private:
  SO3d q_;
  Matrix3d Q_inv_;
};

// AutoDiff cost function (factor) for the difference between a measured 3D
// relative transform, Xij = (tij_, qij_), and the relative transform between two
// estimated poses, Xi_hat and Xj_hat. Weighted by measurement covariance, Qij_.
class RelSE3Factor
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  typedef Matrix<double, 7, 1> Vector7d;
  typedef Matrix<double, 6, 6> Matrix6d;
  // store measured relative pose and inverted covariance matrix
  RelSE3Factor(const Vector7d &X_vec, const Matrix6d &Q)
      : Xij_(X_vec), Q_inv_(Q.inverse())
  {
  }

  // templated residual definition for both doubles and jets
  // basically a weighted implementation of boxminus using Eigen templated types
  template <typename T>
  bool operator()(const T *_Xi_hat, const T *_Xj_hat, T *_res) const
  {
    SE3<T> Xi_hat(_Xi_hat);
    SE3<T> Xj_hat(_Xj_hat);
    Map<Matrix<T, 6, 1>> r(_res);
    r = Q_inv_ * (Xi_hat.inverse() * Xj_hat - Xij_.cast<T>());
    return true;
  }

  static ceres::CostFunction *Create(const Vector7d &Xij, const Matrix6d &Q)
  {
    return new ceres::AutoDiffCostFunction<RelSE3Factor,
                                           6,
                                           7,
                                           7>(new RelSE3Factor(Xij, Q));
  }

private:
  SE3d Xij_;
  Matrix6d Q_inv_;
};

// AutoDiff cost function (factor) for the difference between a range measurement
// rij, and the relative range between two estimated poses, Xi_hat and Xj_hat.
// Weighted by measurement variance, qij_.
class RangeFactor
{
public:
  // store measured range and inverted variance
  RangeFactor(double &rij, double &qij)
  {
    rij_ = rij;
    qij_inv_ = 1.0 / qij;
  }

  // templated residual definition for both doubles and jets
  template <typename T>
  bool operator()(const T *_Xi_hat, const T *_Xj_hat, T *_res) const
  {
    Eigen::Matrix<T, 3, 1> ti_hat(_Xi_hat), tj_hat(_Xj_hat);
    *_res = static_cast<T>(qij_inv_) * (static_cast<T>(rij_) - (tj_hat - ti_hat).norm());
    return true;
  }

  // cost function generator--ONLY FOR PYTHON WRAPPER
  static ceres::CostFunction *Create(double &rij, double &qij)
  {
    return new ceres::AutoDiffCostFunction<RangeFactor,
                                           1,
                                           7,
                                           7>(new RangeFactor(rij, qij));
  }

private:
  double rij_;
  double qij_inv_;
};

// AutoDiff cost function (factor) for the difference between an altitude
// measurement hi, and the altitude of an estimated pose, Xi_hat.
// Weighted by measurement variance, qi_.
class AltFactor
{
public:
  // store measured range and inverted variance
  AltFactor(double &hi, double &qi)
  {
    hi_ = hi;
    qi_inv_ = 1.0 / qi;
  }

  // templated residual definition for both doubles and jets
  template <typename T>
  bool operator()(const T *_Xi_hat, T *_res) const
  {
    T hi_hat = *(_Xi_hat + 2);
    *_res = static_cast<T>(qi_inv_) * (static_cast<T>(hi_) - hi_hat);
    return true;
  }

  // cost function generator--ONLY FOR PYTHON WRAPPER
  static ceres::CostFunction *Create(double &hi, double &qi)
  {
    return new ceres::AutoDiffCostFunction<AltFactor,
                                           1,
                                           7>(new AltFactor(hi, qi));
  }

private:
  double hi_;
  double qi_inv_;
};

// AutoDiff cost function (factor) for time-syncing attitude measurements,
// giving the residual q_ref - (q + dt * w), where dt is the decision
// variable. Weighted by measurement covariance, Q[3x3].
class TimeSyncAttFactor
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  TimeSyncAttFactor(const Vector4d &q_ref_vec, const Vector4d &q_vec,
                    const Vector3d &w_vec, const Matrix3d &Q)
      : q_ref_(q_ref_vec), q_(q_vec), w_(w_vec), Q_inv_(Q.inverse())
  {
  }

  template <typename T>
  bool operator()(const T *_dt_hat, T *_res) const
  {
    Map<Matrix<T, 3, 1>> r(_res);
    r = Q_inv_ * (q_ref_.cast<T>() - (q_.cast<T>() + *_dt_hat * w_.cast<T>()));
    return true;
  }

  static ceres::CostFunction *Create(const Vector4d &q_ref_vec, const Vector4d &q_vec,
                                     const Vector3d &w_vec, const Matrix3d &Q)
  {
    return new ceres::AutoDiffCostFunction<TimeSyncAttFactor,
                                           3,
                                           1>(new TimeSyncAttFactor(q_ref_vec, q_vec,
                                                                    w_vec, Q));
  }

private:
  SO3d q_ref_;
  SO3d q_;
  Vector3d w_;
  Matrix3d Q_inv_;
};

// AutoDiff cost function (factor) for SO3 offset calibration from attitude measurements,
// giving the residual q_ref - (q * q_off), where q_off is the decision variable.
// Weighted by measurement covariance, Q[3x3].
class SO3OffsetFactor
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  SO3OffsetFactor(const Vector4d &q_ref_vec, const Vector4d &q_vec, const Matrix3d &Q)
      : q_ref_(q_ref_vec), q_(q_vec), Q_inv_(Q.inverse())
  {
  }

  template <typename T>
  bool operator()(const T *_q_off, T *_res) const
  {
    SO3<T> q_off(_q_off);
    Map<Matrix<T, 3, 1>> r(_res);
    r = Q_inv_ * (q_ref_.cast<T>() - (q_.cast<T>() * q_off));
    return true;
  }

  static ceres::CostFunction *Create(const Vector4d &q_ref_vec, const Vector4d &q_vec,
                                     const Matrix3d &Q)
  {
    return new ceres::AutoDiffCostFunction<SO3OffsetFactor,
                                           3,
                                           4>(new SO3OffsetFactor(q_ref_vec, q_vec, Q));
  }

private:
  SO3d q_ref_;
  SO3d q_;
  Matrix3d Q_inv_;
};

// AutoDiff cost function (factor) for SE3 offset calibration from pose measurements,
// giving the residual T_ref - (T * T_off), where T_off is the decision variable.
// Weighted by measurement covariance, Q[6x6].
class SE3OffsetFactor
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  typedef Matrix<double, 7, 1> Vector7d;
  typedef Matrix<double, 6, 6> Matrix6d;
  SE3OffsetFactor(const Vector7d &T_ref_vec, const Vector7d &T_vec, const Matrix6d &Q)
      : T_ref_(T_ref_vec), T_(T_vec), Q_inv_(Q.inverse())
  {
  }

  template <typename T>
  bool operator()(const T *_T_off, T *_res) const
  {
    SE3<T> T_off(_T_off);
    Map<Matrix<T, 6, 1>> r(_res);
    r = Q_inv_ * (T_ref_.cast<T>() - (T_.cast<T>() * T_off));
    return true;
  }

  static ceres::CostFunction *Create(const Vector7d &T_ref_vec, const Vector7d &T_vec,
                                     const Matrix6d &Q)
  {
    return new ceres::AutoDiffCostFunction<SE3OffsetFactor,
                                           6,
                                           7>(new SE3OffsetFactor(T_ref_vec, T_vec, Q));
  }

private:
  SE3d T_ref_;
  SE3d T_;
  Matrix6d Q_inv_;
};

class SE3ReprojectionFactor
{
public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW
  SE3ReprojectionFactor(
      const double &fx,
      const double &fy,
      const double &cx,
      const double &cy,
      const Vector2f &img_coords,
      const Vector3f &world_coords) : _fx(fx),
                                      _fy(fy),
                                      _cx(cx),
                                      _cy(cy),
                                      _img_coords(img_coords),
                                      _world_coords(world_coords) {}

  template <typename T>
  bool operator()(const T *_H, T *res) const
  {
    SE3<T> H(_H);
    Map<Matrix<T, 2, 1>> r(res);
    Matrix<T, 3, 1> camera_coords = H.inverse() * _world_coords.cast<T>();
    Matrix<T, 2, 1> proj;
    proj << (T)_fx * camera_coords.x() / camera_coords.z() + (T)_cx,
        (T)_fy * camera_coords.y() / camera_coords.z() + (T)_cy;
    r = _img_coords.cast<T>() - proj;
    return true;
  }

  static ceres::CostFunction *Create(
      const double &fx,
      const double &fy,
      const double &cx,
      const double &cy,
      const Vector2f &img_coords,
      const Vector3f &world_coords)
  {
    return new ceres::AutoDiffCostFunction<SE3ReprojectionFactor,
                                           2,
                                           7>(new SE3ReprojectionFactor(fx,
                                                                        fy,
                                                                        cx,
                                                                        cy,
                                                                        img_coords,
                                                                        world_coords));
  }

private:
  const Vector2f _img_coords;
  const Vector3f _world_coords;
  const double _fx;
  const double _fy;
  const double _cx;
  const double _cy;
};