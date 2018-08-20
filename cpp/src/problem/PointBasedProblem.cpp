#include <polyfem/PointBasedProblem.hpp>
#include <polyfem/State.hpp>
#include <polyfem/MatrixUtils.hpp>

#include <iostream>

namespace polyfem
{
	bool PointBasedTensorProblem::BCValue::init(const json &data)
	{
		Eigen::Matrix<bool, 3, 1> dd; dd.setConstant(true);
		bool all_dimentions_dirichelt = true;

		if(data.is_array())
		{
			assert(data.size() == 3);
			init((double)data[0], (double)data[1], (double)data[2], dd);
			//TODO add dimension
		}
		else if(data.is_object())
		{
			Eigen::MatrixXd fun, pts;
			Eigen::MatrixXi tri;
			read_matrix(data["function"], fun);
			read_matrix(data["points"], pts);
			pts = pts.block(0, 0, pts.rows(), 2).eval();
			read_matrix(data["triangles"], tri);

			const int coord = data["coordinate"];
			if(data.find("dimension") != data.end())
			{
				all_dimentions_dirichelt = false;
				auto &tmp = data["dimension"];
				assert(tmp.is_array());
				for(size_t k = 0; k < tmp.size(); ++k)
					dd(k) = tmp[k];
			}

			init(pts, tri, fun, coord, dd);
		}
		else
		{
			init(0, 0, 0, dd);
		}

		return all_dimentions_dirichelt;
	}

	Eigen::RowVector3d PointBasedTensorProblem::BCValue::operator()(const Eigen::RowVector3d &pt) const
	{
		if(is_val)
		{
			return val.transpose();
		}
		Eigen::RowVector2d pt2; pt2 << pt(coordiante_0), pt(coordiante_1);
		Eigen::RowVector3d res;


		res = func.interpolate(pt2);

		return res;
	}

	PointBasedTensorProblem::PointBasedTensorProblem(const std::string &name)
	: Problem(name), rhs_(0), scaling_(1)
	{
		translation_.setZero();
	}

	void PointBasedTensorProblem::rhs(const std::string &formulation, const Eigen::MatrixXd &pts, const double t, Eigen::MatrixXd &val) const
	{
		val = Eigen::MatrixXd::Constant(pts.rows(), pts.cols(), rhs_);
		val *= t;
	}

	void PointBasedTensorProblem::bc(const Mesh &mesh, const Eigen::MatrixXi &global_ids, const Eigen::MatrixXd &pts, const double t, Eigen::MatrixXd &val) const
	{
		val = Eigen::MatrixXd::Zero(pts.rows(), mesh.dimension());

		for(long i = 0; i < pts.rows(); ++i)
		{
			const int id = mesh.get_boundary_id(global_ids(i));
			const auto &pt3d = (pts.row(i) + translation_.transpose())/scaling_;
			for(size_t b = 0; b < boundary_ids_.size(); ++b)
			{
				if(id == boundary_ids_[b])
				{
					val.row(i) = bc_[b](pt3d)*scaling_;
				}
			}
		}

		val *= t;
	}

	void PointBasedTensorProblem::add_constant(const int bc_tag, const Eigen::Vector3d &value, const Eigen::Matrix<bool, 3, 1> &dd)
	{
		all_dimentions_dirichelt_ = all_dimentions_dirichelt_ && dd(0) && dd(1) && dd(2);
		boundary_ids_.push_back(bc_tag);
		bc_.emplace_back();
		bc_.back().init(value, dd);
	}

	void PointBasedTensorProblem::add_function(const int bc_tag, const Eigen::MatrixXd &func, const Eigen::MatrixXd &pts, const Eigen::MatrixXi &tri, const int coord, const Eigen::Matrix<bool, 3, 1> &dd)
	{
		all_dimentions_dirichelt_ = all_dimentions_dirichelt_ && dd(0) && dd(1) && dd(2);

		boundary_ids_.push_back(bc_tag);
		bc_.emplace_back();
		bc_.back().init(pts.block(0, 0, pts.rows(), 2), tri, func, coord, dd);
	}

	bool PointBasedTensorProblem::is_dimention_dirichet(const int tag, const int dim) const
	{
		if(all_dimentions_dirichelt())
			return true;

		for(size_t b = 0; b < boundary_ids_.size(); ++b)
		{
			if(tag == boundary_ids_[b])
			{
				return bc_[b].is_dirichet_dim(dim);
			}
		}

		assert(false);
		return true;
	}

	void PointBasedTensorProblem::set_parameters(const json &params)
	{
		if(initialized_)
			return;

		if(params.find("scaling") != params.end())
		{
			scaling_ = params["scaling"];
		}

		if(params.find("rhs") != params.end())
		{
			rhs_ = params["rhs"];
		}

		if(params.find("translation") != params.end())
		{
			const auto &j_translation = params["translation"];

			assert(j_translation.is_array());
			assert(j_translation.size() == 3);

			for(int k = 0; k < 3; ++k)
				translation_(k) = j_translation[k];
		}

		if(params.find("boundary_ids") != params.end())
		{
			boundary_ids_.clear();
			auto j_boundary = params["boundary_ids"];

			boundary_ids_.resize(j_boundary.size());
			bc_.resize(boundary_ids_.size());

			for(size_t i = 0; i < boundary_ids_.size(); ++i)
			{
				boundary_ids_[i] = j_boundary[i]["id"];
				const auto ff = j_boundary[i]["value"];
				const bool all_d = bc_[i].init(ff);
				all_dimentions_dirichelt_ = all_dimentions_dirichelt_ && all_d;

			}
		}
	}
}
