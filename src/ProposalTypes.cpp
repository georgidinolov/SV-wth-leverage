#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_vector.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_linalg.h>
#include <iostream>
#include <cmath>
#include <limits>
#include "MultivariateNormal.hpp"
#include "ProposalTypes.hpp"

// ================== ThetaTauSquareRhoProposal ==================
ThetaTauSquareRhoProposal::ThetaTauSquareRhoProposal()
  : proposal_covariance_matrix_(gsl_matrix_alloc (3, 3)),
    theta_upper_bound_(std::numeric_limits<double>::infinity()),
    theta_lower_bound_(-1.0*std::numeric_limits<double>::infinity())
{
  gsl_matrix_set_zero(proposal_covariance_matrix_);
  for (int i=0; i<3; ++i) {
    gsl_matrix_set (proposal_covariance_matrix_, i, i, 1.0);
  }
}

ThetaTauSquareRhoProposal::
ThetaTauSquareRhoProposal(const gsl_matrix * proposal_covariance_matrix)
  : proposal_covariance_matrix_(gsl_matrix_alloc (3, 3)),
    theta_upper_bound_(std::numeric_limits<double>::infinity()),
    theta_lower_bound_(-1.0*std::numeric_limits<double>::infinity())
{
  gsl_matrix_memcpy(proposal_covariance_matrix_, proposal_covariance_matrix);
}

ThetaTauSquareRhoProposal::~ThetaTauSquareRhoProposal()
{
  gsl_matrix_free(proposal_covariance_matrix_);
}

const gsl_matrix * ThetaTauSquareRhoProposal::get_proposal_covariance_matrix() const
{
  return proposal_covariance_matrix_;
}

void ThetaTauSquareRhoProposal::set_theta_upper_bound(double upper_bound)
{
  theta_upper_bound_ = upper_bound;
}

void ThetaTauSquareRhoProposal::set_theta_lower_bound(double lower_bound)
{
  theta_lower_bound_ = lower_bound;
}

std::vector<double> ThetaTauSquareRhoProposal::
propose_parameters(const gsl_rng *r, 
		   const std::vector<double>& mean) const
{
  gsl_vector * result = gsl_vector_alloc(3);
  gsl_vector * transformed_mean = gsl_vector_alloc(3);

  double rho_current = mean[0];
  gsl_vector_set(transformed_mean, 0, log( ((rho_current+1.0)/2.0) /
					   (1 - (rho_current+1.0)/2.0) ) );

  double theta_current = mean[1];
  gsl_vector_set(transformed_mean, 1, log(theta_current / (1-theta_current)));

  double tau_square_current = mean[2];
  gsl_vector_set(transformed_mean, 2, log(tau_square_current));
  
  rmvnorm(r, 
	  3,
	  transformed_mean, 
	  proposal_covariance_matrix_,
	  result);

  double rho_tilde_proposed = gsl_vector_get(result, 0);
  double rho = 2.0*exp(rho_tilde_proposed)/(1.0+exp(rho_tilde_proposed)) - 1.0;
  
  double theta_tilde_proposed = gsl_vector_get(result, 1);
  double theta = exp(theta_tilde_proposed)/(1.0+exp(theta_tilde_proposed));

  double tau_square_tilde_proposed = gsl_vector_get(result, 2);
  double tau_square = exp(tau_square_tilde_proposed);

  while (theta <= theta_lower_bound_ || theta >= theta_upper_bound_ ) {
    std::cout << "Re-trying proposal" << std::endl;
    rmvnorm(r, 
	    3,
	    transformed_mean, 
	    proposal_covariance_matrix_,
	    result);
    
    rho_tilde_proposed = gsl_vector_get(result, 0);
    rho = 2.0*exp(rho_tilde_proposed)/(1.0+exp(rho_tilde_proposed)) - 1.0;
    
    theta_tilde_proposed = gsl_vector_get(result, 1);
    theta = exp(theta_tilde_proposed)/(1.0+exp(theta_tilde_proposed));
    
    tau_square_tilde_proposed = gsl_vector_get(result, 2);
    tau_square = exp(tau_square_tilde_proposed);
  }
  
  gsl_vector_free(result);
  gsl_vector_free(transformed_mean);
  
  std::vector<double> out {rho, theta, tau_square};
  return out;
}


// TILDE 
// ================== RhoThetaTauSquareTildeProposal ==================
RhoThetaTauSquareTildeProposal::
RhoThetaTauSquareTildeProposal(const gsl_vector * proposal_mean,
			       const gsl_matrix * proposal_covariance_matrix,
			       int dof)
  : proposal_tilde_mean_(gsl_vector_alloc (3)),
    proposal_tilde_covariance_matrix_(gsl_matrix_alloc (3, 3)),
    dof_(dof)
{
  gsl_vector_memcpy(proposal_tilde_mean_, proposal_mean);
  gsl_matrix_memcpy(proposal_tilde_covariance_matrix_, proposal_covariance_matrix);
}

RhoThetaTauSquareTildeProposal::~RhoThetaTauSquareTildeProposal()
{
  gsl_vector_free(proposal_tilde_mean_);
  gsl_matrix_free(proposal_tilde_covariance_matrix_);
}

const gsl_vector * RhoThetaTauSquareTildeProposal::get_proposal_mean() const
{
  return proposal_tilde_mean_;
}

const gsl_matrix * RhoThetaTauSquareTildeProposal::get_proposal_covariance_matrix() const
{
  return proposal_tilde_covariance_matrix_;
}

std::vector<double> RhoThetaTauSquareTildeProposal::
propose_parameters(const gsl_rng *r) const
{
  gsl_vector * result = gsl_vector_alloc(3);
  
  rmvt(r, 
       3,
       proposal_tilde_mean_, 
       proposal_tilde_covariance_matrix_,
       dof_,
       result);

  double rho_tilde_proposed = gsl_vector_get(result, 0);
  double theta_tilde_proposed = gsl_vector_get(result, 1);
  double tau_square_tilde_proposed = gsl_vector_get(result, 2);

  gsl_vector_free(result);
  
  std::vector<double> out {rho_tilde_proposed, 
      theta_tilde_proposed, 
      tau_square_tilde_proposed};
  return out;
}

double RhoThetaTauSquareTildeProposal::
q_log_likelihood(const std::vector<double>& proposed_tilde) const
{
  gsl_vector * proposed_tilde_ptr = gsl_vector_alloc(3);
  gsl_vector_set(proposed_tilde_ptr, 0, proposed_tilde[0]);
  gsl_vector_set(proposed_tilde_ptr, 1, proposed_tilde[1]);
  gsl_vector_set(proposed_tilde_ptr, 2, proposed_tilde[2]);

  double out = dmvt_log(3, 
			proposed_tilde_ptr, 
			proposal_tilde_mean_, 
			proposal_tilde_covariance_matrix_,
			dof_);

  gsl_vector_free(proposed_tilde_ptr);
  return out;
}

// =======================================================
NormalRWProposal::NormalRWProposal(int d, 
				   const gsl_matrix * proposal_covariance_matrix)
  : d_(d),
    proposal_covariance_matrix_(gsl_matrix_alloc (d, d))
{
  gsl_matrix_memcpy(proposal_covariance_matrix_, proposal_covariance_matrix);
}

NormalRWProposal::~NormalRWProposal()
{
  gsl_matrix_free(proposal_covariance_matrix_);
}

std::vector<double> NormalRWProposal::
propose_parameters(const gsl_rng *r, 
		   const std::vector<double>& mean) const
{
  gsl_vector * result = gsl_vector_alloc(d_);
  gsl_vector * gsl_mean = gsl_vector_alloc(d_);

  for (int i=0; i<d_; ++i) {
    gsl_vector_set(gsl_mean, i, mean[i]);
  }
  
  rmvnorm(r, 
	  d_,
	  gsl_mean, 
	  proposal_covariance_matrix_,
	  result);

  std::vector<double> out = std::vector<double> (d_);
  for (int i=0; i<d_; ++i) {
    out[i] = gsl_vector_get(result,i);
  }

  gsl_vector_free(result);
  gsl_vector_free(gsl_mean);

  return out;
}

std::vector<double> NormalRWProposal::
propose_parameters(const gsl_rng *r, 
		   const int d,
		   const std::vector<double>& mean,
		   const gsl_matrix * proposal_covariance_matrix) const
{
  gsl_vector * result = gsl_vector_alloc(d_);
  gsl_vector * gsl_mean = gsl_vector_alloc(d_);

  for (int i=0; i<d_; ++i) {
    gsl_vector_set(gsl_mean, i, mean[i]);
  }
  
  rmvnorm(r, 
	  d,
	  gsl_mean, 
	  proposal_covariance_matrix,
	  result);

  std::vector<double> out = std::vector<double> (d_);
  for (int i=0; i<d_; ++i) {
    out[i] = gsl_vector_get(result,i);
  }

  gsl_vector_free(result);
  gsl_vector_free(gsl_mean);

  return out;
}

std::vector<double> NormalRWProposal::
propose_parameters(const gsl_rng *r, 
		   const int d,
		   const std::vector<double>& mean,
		   const gsl_matrix * proposal_covariance_matrix,
		   const int dof) const
{
  gsl_vector * result = gsl_vector_alloc(d_);
  gsl_vector * gsl_mean = gsl_vector_alloc(d_);

  for (int i=0; i<d_; ++i) {
    gsl_vector_set(gsl_mean, i, mean[i]);
  }
  
  rmvt(r, 
       d,
       gsl_mean, 
       proposal_covariance_matrix,
       dof,
       result);

  std::vector<double> out = std::vector<double> (d_);
  for (int i=0; i<d_; ++i) {
    out[i] = gsl_vector_get(result,i);
  }

  gsl_vector_free(result);
  gsl_vector_free(gsl_mean);

  return out;
}