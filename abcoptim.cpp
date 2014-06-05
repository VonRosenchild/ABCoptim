#include <Rcpp.h>

using namespace Rcpp;

/* Constant definitions */
#define MAX_FOODNUMBER 100
#define MAX_D 100

/* Structures definition */
struct abc_boundaries {
  double lb[MAX_FOODNUMBER*MAX_D];
  double ub[MAX_FOODNUMBER*MAX_D];
} ;

/* Calculates runif */
// [[Rcpp::export]]
double abc_unif(double minval=0.0, double maxval=1.0)
{
  return ((double)rand()/(double)RAND_MAX)*(maxval - minval) + minval;
}

// [[Rcpp::export]]
double abc_calc_fit(double x) {  
	if (x >= 0) return 1/(x + 1);
	else return(1 + fabs(x));
}

// The best food source is memorized
void abc_mem_best_src(
  List Foods,
  int * persistance)
{
	// Poiting to the foods position en valfun
	NumericMatrix foods_pos = Foods["pos"];
	NumericVector foods_val = Foods["val"];
	int nfoods = foods_val.size();
	int nparam = foods_pos.ncol();

	/* Pointing to the globmin pos and valfun */
	NumericVector global_pos  = Foods["GlobalParams"];
	NumericVector global_val  = Foods["GlobalMin"];

	int change = 0;
	for(int i=0;i<nfoods;i++)
		if (foods_val[i] < global_val[0]) {
      			change+=1;
			/* Found a new global minimum */
			global_val[0] = foods_val[i];
			for(int j=0;j<nparam;j++)
				global_pos[j] = foods_pos(i,j);
        
		}
	if (!change) *persistance+=1;

	return ;
}

// [[Rcpp::export]]
double abc_fun(Function targetfun, NumericVector par) {
	return as<double>(targetfun(par));
}

List abc_initialize(
  int nfoods,
  int nparam,
  Function objfun,
  double * lb,
  double * ub,
  int limit,
  double * positions
) 
{
  NumericVector fitness(nfoods);
	NumericMatrix pos(nfoods,nparam);
	NumericVector val(nfoods);
	NumericVector prob(nfoods);
  NumericVector trials(nfoods);
  NumericVector GlobPars(nparam);
  
  for(int i=0;i<nfoods;i++)
  {
    for(int j=0;j<nparam;j++)
    {
      pos(i,j) = abc_unif(lb[j],ub[j]);
      positions[i*nparam+j] = pos(i,j);
    }
      
    val[i]     = as<double>(objfun(pos(i,_)));
    fitness[i] = abc_calc_fit(val[i]);
  }
  

	return List::create(
    _["fitness"]      = fitness,  /* Fitness of each food source */
    _["pos"]          = pos,      /* Current possition of each food source */
    _["prob"]         = prob,
    _["val"]          = val,      /* Current value of each food source */
    _["GlobalMin"]    = val[0],   /* Current Global Minimum */
    _["GlobalParams"] = pos(0,_), /* Current optimal params */
    _["trials"]       = trials,   /* Number of trials on the source */
    _["persistance"]  = 0,        /* N of iter without improvement */
    _["limit"]        = limit     /* Maximum number of attemps */
	);
} 

void abc_init(
  int index,
	List Foods,
	Function objfun,
	double * lb,
	double * ub
) {
	int i,j;

	/* Getting the food number */
	NumericMatrix foods_pos = Foods["pos"];
	NumericVector foods_val = Foods["val"];
	NumericVector foods_trl = Foods["trials"];
	NumericVector foods_fit = Foods["fitness"];
	int nparam = foods_pos.ncol();

	/* Setting the positions */
	for(j=0;j<nparam;j++)
		foods_pos(index,j) = abc_unif(lb[j],ub[j]);
	
	/* Getting the value and */
	foods_val[index] = abc_fun(objfun, foods_pos(index,_));
	foods_fit[index] = abc_calc_fit(foods_val[index]);
	foods_trl[index] = 0;

	return ;
}

void abc_send_empl_bees(
  List Foods, 
  int nfoods, int nparam,
  double * lb, double * ub, 
  Function objfun
) {
	/* Poiting */
	NumericMatrix foods_pos = Foods["pos"];
	NumericVector foods_val = Foods["val"];
  NumericVector foods_fit = Foods["fitness"];
  NumericVector foods_trl = Foods["trials"];

	NumericVector new_foods_pos;

	int param2change = floor(abc_unif()*nparam);
  
	/* Random solution */
	int neighbour  = 0;
  double new_val = 0.0;
  double new_fit = 0.0;
  
	for(int i=0;i<nfoods;i++) 
  {
    neighbour = (int)floor(abc_unif()*nfoods);
		while(neighbour==i)
    {
      /* Checking if the user press break */
      // R_CheckUserInterrupt();
      neighbour = (int)floor(abc_unif()*nfoods);
      
    }
    new_foods_pos = foods_pos(i,_);
		new_foods_pos[param2change] =
			foods_pos(i,param2change) +
			(foods_pos(i,param2change) - 
			foods_pos(neighbour,param2change)*(abc_unif()-.5)*2);
    
    /* Checking boundaries */
    if (new_foods_pos[param2change] > ub[param2change]) 
      new_foods_pos[param2change] = ub[param2change];
    else if (new_foods_pos[param2change] < lb[param2change]) 
      new_foods_pos[param2change] = lb[param2change];      
    
    new_val = abc_fun(objfun,new_foods_pos);
    new_fit = abc_calc_fit(new_val);
    
    /* Greedy selection */
    if (new_fit > foods_fit[i])
    {
      foods_trl[i] = 0;
      foods_pos(i,_) = clone(new_foods_pos);
      foods_val[i]   = new_val;
      foods_fit[i]   = new_fit;
    }
    else foods_trl[i] += 1;
  }

	return ;
}

/* A food source is chosen with the probability which is proportioal to its 
quality. Different schemes can be used to calculate the probability values For
example prob(i)=fitness(i)/sum(fitness) or in a way used in the metot below
prob(i)=a*fitness(i)/max(fitness)+b probability values are calculated by using
fitness values and normalized by dividing maximum fitness value
*/
// [[Rcpp::export]]
void abc_calc_prob(List Foods,int FoodNumber) {
  NumericVector fitness = Foods["fitness"];
  NumericVector prob    = Foods["prob"];
  double maxfit  = fitness[0];
  
  for(int i=1;i<FoodNumber;i++) 
    if (fitness[i] > maxfit) maxfit = fitness[i];
  
  for(int i=0;i<FoodNumber;i++)
    prob[i] = (0.9*(fitness[i]/maxfit) + .1);
    
  return ;
}

/* Pointer Function Definition */
/*type double (*ptrFun)(NumericVector x);*/
void abc_send_onlooker_bees(
  List Foods, 
  Function objfun,
  int FoodNumber,
  int D,
  double * lb,
  double * ub
  )
{
  // Onlooker Bee phase
  int i = 0;
  int t = 0;
  
  NumericVector foods_val = Foods["val"];
  NumericMatrix foods_pos = Foods["pos"];
  NumericVector foods_trl = Foods["trials"];
  NumericVector foods_prb = Foods["prob"];
  // Rprintf("ok\n");
  NumericVector foods_fit = Foods["fitness"];
  NumericVector solution(D);  
  double r=0.0;
  while(t < FoodNumber)
  {
    /* Checking if the user press break */
    R_CheckUserInterrupt();
    
    r = abc_unif();
    // choose a food source depending on its probability to be chosen
    if (r < foods_prb[i])
    {
      t += 1;
      r = abc_unif();
      
      // The parameter to be changed is determined randomly
      int param2change = floor(r*D);
      
      /* A randomly chosen solution is used in producing a mutant solution of
      the solution i Randomly selected solution must be different from the
      solution i*/        
      int neighbour = i;
      while(neighbour==i)
        neighbour = floor(abc_unif()*FoodNumber);

      solution = foods_pos(i,_);
      
      /* v_{ij}=x_{ij}+\phi_{ij}*(x_{kj}-x_{ij}) */
      r = abc_unif();
      
      /* if (optiinteger) solution[param2change] <<- r > .5
      else 
      {*/
      solution[param2change] =
        solution[param2change]+
        (solution[param2change]-foods_pos[neighbour,param2change])*(r-0.5)*2;
      
      /* if generated parameter value is out of boundaries, it is shifted onto
      the boundaries*/
      if (solution[param2change]<lb[param2change]) 
        solution[param2change] = lb[param2change];
      
      if (solution[param2change]>ub[param2change]) 
        solution[param2change] = ub[param2change];
        
      /*}*/
      
      double ObjValSol  = as<double>(objfun(solution));
      double FitnessSol = abc_calc_fit(ObjValSol);
      
      /* a greedy selection is applied between the current solution i and its
      mutant*/
      if (FitnessSol>foods_fit[i])
      {
        /* If the mutant solution is better than the current solution i,
        replace the solution with the mutant and reset the trial counter of
        solution i*/
        foods_trl[i]   = 0;
        foods_pos(i,_) = clone(solution);
        foods_val[i]   = ObjValSol;
        foods_fit[i]   = FitnessSol;
      } /* if the solution i can not be improved, increase its trial counter*/
      else foods_trl[i] += 1;
    }
    if (i<FoodNumber) i+= 1;
    else i = 0;
    
    // end of onlooker bee phase
  }
}

/* determine the food sources whose trial counter exceeds the "limit" value.
In Basic ABC, only one scout is allowed to occur in each cycle*/
void abc_send_scout_bees(
  List Foods,
  Function objfun,
  double * ub, double * lb,
  int FoodNumber)
{
  NumericVector trials = Foods["trials"];
  int maxtrialindex    = 1;
  int limit            = Foods["limit"];
  for (int i=0;i<FoodNumber;i++)
    if (trials[i] > trials[maxtrialindex]) maxtrialindex = i;
  
  if (trials[maxtrialindex] >= limit)
    abc_init(maxtrialindex, Foods, objfun, lb, ub);
  
  
  return;
}

// [[Rcpp::export]]
List abc_optimCpp(
  NumericVector par, Function fn,
  NumericVector lb = NumericVector::create(1,-DBL_MAX),
  NumericVector ub = NumericVector::create(1,DBL_MAX),
  int FoodNumber   = 20,
  int limit        = 100,
  int maxCycle     = 1000,
  int criter       = 50
  )
{

  /* Number of parameters  */
  int nparam = par.size();
  
  /* Boundaries */
  double lb0[nparam];
  double ub0[nparam];
  if (lb.size() == nparam)
    for(int i=0;i<nparam;i++)
      lb0[i] = lb[i];
  else
    for(int i=0;i<nparam;i++)
      lb0[i] = lb[0];
  
  if (ub.size() == nparam)
    for(int i=0;i<nparam;i++)
      ub0[i] = ub[i];
  else
    for(int i=0;i<nparam;i++)
      ub0[i] = ub[0];
  
  double positions[FoodNumber*nparam];
  List Foods = abc_initialize(FoodNumber, nparam, fn, lb0, ub0, limit, positions);
  abc_calc_prob(Foods, FoodNumber);
  
  for(int i=0;i<FoodNumber;i++)
    for(int j=0;j<nparam;j++)
      if ((j+1)==nparam) Rprintf("\t %5.2f\n",positions[i*nparam+j]);
      else Rprintf("\t %5.2f",positions[i*nparam+j]);
 
  /* Memorizes the initial sol */
  int persistance=0;
  abc_mem_best_src(Foods, &persistance);
  
  /* Start! */
  int iter = 0;
  while (++iter < maxCycle)
  {
    // Rprintf("Enter %d\n", iter);
    /* Checking if the user press break */
    R_CheckUserInterrupt();
    
    abc_send_empl_bees(Foods, FoodNumber, nparam, lb0, ub0, fn);
    // Rprintf("send_empl... Iter %d\n",iter);
    
    abc_calc_prob(Foods, FoodNumber);
    // Rprintf("calc_prob... Iter %d\n",iter);
    
    abc_send_onlooker_bees(Foods, fn, FoodNumber, nparam, lb0, ub0);
    // Rprintf("send_onlooker... Iter %d\n",iter);
    
    abc_mem_best_src(Foods, &persistance);
    // Rprintf("mem_best... Iter %d\n",iter);
    
    if (persistance > criter) break;
    abc_send_scout_bees(Foods, fn, lb0, ub0, FoodNumber);
    // Rprintf("send_scout... Iter %d\n",iter);
  }

  return(
    List::create(
      _["par"]=Foods["GlobalParams"],
      _["value"]=Foods["GlobalMin"],
      _["counts"]=iter,
      _["persistance"]=persistance
      )
    );
}

/*** R

fun <- function(x) {
  -cos(x[1])*cos(x[2])*exp(-((x[1] - pi)^2 + (x[2] - pi)^2))
}

library(microbenchmark)
library(ABCoptim)
microbenchmark(
  abc_optimCpp(par=rep(0,2), fn=fun, lb=-20,
               ub=20, criter=100),
  abc_optim(par=rep(0,2), fn=fun, lb=-20,
  ub=20, criter=100), times=1)


*/

/* // [[Rcpp::export]] */


