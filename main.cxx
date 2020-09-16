// The following simple example illustrates how multiple imaging modalities can
// be registered using the ITK registration framework. The first difference
// between this and previous examples is the use of the
// MutualInformationImageToImageMetric as the cost-function to be
// optimized. The second difference is the use of the
// GradientDescentOptimizer. Due to the stochastic nature of the
// metric computation, the values are too noisy to work successfully with the
// RegularStepGradientDescentOptimizer.  Therefore, we will use the
// simpler GradientDescentOptimizer with a user defined learning rate.  The
// following headers declare the basic components of this registration method.
#include <stdlib.h>

#include "itkVersorRigid3DTransform.h"

//#include "itkMattesMutualInformationImageToImageMetric.h"
// Mattes������û��ʹ��
//#include "itkMultiResolutionImageRegistrationMethod.h"

#include <iostream>
//#include <itkImageToVTKImageFilter.h> 
//#include <itkVTKImageToImageFilter.h> 

#include "vtkSmartPointer.h"
#include "vtkImageViewer2.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkImageBlend.h"


#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include "itkImageSeriesReader.h"

#include "itkImageRegistrationMethod.h"
#include "itkRigid3DTransform.h"
#include "itkMutualInformationImageToImageMetric.h"
#include "itkVersorRigid3DTransformOptimizer.h"
#include "itkCenteredTransformInitializer.h"

#include "itkMinimumMaximumImageCalculator.h"

//  ��׼���ܼ򻯻���Ϣ�ļ���
#include "itkNormalizeImageFilter.h"
//  ��ͨ�˲�ȥ������������³����
#include "itkDiscreteGaussianImageFilter.h"



#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkGradientDescentOptimizer.h"
#include "itkTranslationTransform.h"
#include "itkResampleImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkCheckerBoardImageFilter.h"


//  The following section of code implements a Command observer
//  that will monitor the evolution of the registration process.
//
#include "itkCommand.h"

using namespace std;

constexpr unsigned int Dimension = 3;
using PixelType = float;
using FixedImageType = itk::Image<PixelType, Dimension>;
using MovingImageType = itk::Image<PixelType, Dimension>;


//  ����Ϣ�ڱ�׼�������̬�ֲ����и��õ����ܣ���fixed image �� moving image����׼��Ϊinternal images
using InternalPixelType = float;
using InternalImageType = itk::Image<InternalPixelType, Dimension>;

using TransformType = itk::VersorRigid3DTransform< double >;
using OptimizerType = itk::VersorRigid3DTransformOptimizer;
using InterpolatorType = itk::LinearInterpolateImageFunction<InternalImageType, double>;
using RegistrationType = itk::ImageRegistrationMethod<InternalImageType, InternalImageType>;
using MetricType = itk::MutualInformationImageToImageMetric<InternalImageType, InternalImageType>;

class CommandIterationUpdate : public itk::Command
{
public:
	// ITK�ٷ���׼�����ͱ�����
	using Self = CommandIterationUpdate;
	using Superclass = itk::Command;
	using Pointer = itk::SmartPointer<Self>;
	itkNewMacro(Self);

protected:
	CommandIterationUpdate() = default;
	;

public:
	using OptimizerType = itk::VersorRigid3DTransformOptimizer;
	//constָ�룬��ָ֤���Optimizer���󲻻ᱻ�޸�
	using OptimizerPointer = const OptimizerType *;

	//˫Execute()���������ã�������������ӹ۲��ߵ��Ƿ�const��Optimizer��
	//�����Ҫ�ڵ��÷�const�ķ���������Լ�д�õ�const���󷽷�
	void
		Execute(itk::Object * caller, const itk::EventObject & event) override
	{
		//ǿ��ת����const Optimizer -> const Optimizer�����ܱ�֤Optimizer���ᱻ�����޸�
		Execute((const itk::Object *)caller, event);
	}

	void
		Execute(const itk::Object * object, const itk::EventObject & event) override
	{
		auto optimizer = dynamic_cast<OptimizerPointer>(object);
		if (!itk::IterationEvent().CheckEvent(&event))
		{
			return;
		}
		std::cout << optimizer->GetCurrentIteration() << "   ";
		std::cout << optimizer->GetValue() << "   ";
		std::cout << optimizer->GetCurrentStepLength() << "   ";
		std::cout << optimizer->GetCurrentPosition() << std::endl;
		
	}
};

// C3L-03728
const char * fixedImageFile = "D:/03_Program/CPTAC-GBM/C3L-03728/09-16-2001-CT HEAD WO CONTRAST-48550/2-HEAD STD-06783";
const char * movingImageFile = "D:/03_Program/CPTAC-GBM/C3L-03728/09-16-2001-MR BRAIN FOR STEREOTACTIC WWO CONTR-56599/301-AX T2 MVXD-73717";

// C3L-02465 ��һ�飬Ч����̫����
//const char * fixedImageFile = "D:/03_Program/CPTAC-GBM/C3L-02465/01-24-2001-CT HEAD WO CONTRAST-26900/3-HEAD BW-64764";
//const char * movingImageFile = "D:/03_Program/CPTAC-GBM/C3L-02465/01-23-2001-MR BRAIN FOR STEREOTACTIC WWO CONTR-85523/5-Ax T2 Flair-33243";

// C3L-02465 �ڶ���
//const char* fixedImageFile = "D:/03_Program/CPTAC-GBM/C3L-02465/01-15-2001-CT ANGIO HEAD W CON W WAND-16913/2-HEAD STD-64164";
//const char* movingImageFile = "D:/03_Program/CPTAC-GBM/C3L-02465/01-15-2001-MR BRAIN WOW CONTRAST-56516/7-OPT AX T2 FSE inter-45677";


// ��ȡDICOM������
void readDICOMImage(FixedImageType::Pointer& fixedImage, MovingImageType::Pointer& movingImage)
{

	using FixedImageReaderType = itk::ImageSeriesReader<FixedImageType>;
	using MovingImageReaderType = itk::ImageSeriesReader<MovingImageType>;
	FixedImageReaderType::Pointer  fixedImageReader = FixedImageReaderType::New();
	MovingImageReaderType::Pointer movingImageReader = MovingImageReaderType::New();

	using ImageIOType = itk::GDCMImageIO;
	ImageIOType::Pointer dicomIO = ImageIOType::New();
	fixedImageReader->SetImageIO(dicomIO);
	movingImageReader->SetImageIO(dicomIO);
	//fixedImageReader->ForceOrthogonalDirectionOff(); // properly read CTs with gantry tilt
	//movingImageReader->ForceOrthogonalDirectionOff(); // properly read CTs with gantry tilt
	// �ʺ϶�ȡ���ż���б��CTͼ��

	using NamesGeneratorType = itk::GDCMSeriesFileNames;
	NamesGeneratorType::Pointer fixedNameGenerator = NamesGeneratorType::New();
	fixedNameGenerator->SetUseSeriesDetails(true);
	fixedNameGenerator->SetDirectory(fixedImageFile);
	fixedNameGenerator->AddSeriesRestriction("0008|0021"); // tag: series date ��֤��ͬһ������


	NamesGeneratorType::Pointer movingNameGenerator = NamesGeneratorType::New();
	movingNameGenerator->SetUseSeriesDetails(true);
	movingNameGenerator->SetDirectory(movingImageFile);
	movingNameGenerator->AddSeriesRestriction("0008|0021");

	try
	{
		std::cout << std::endl;

		// UID ( Unique Identifiers ) Ψһ��־ֵ��ÿ������ӵ��Ψһ�ı�־ֵ
		using SeriesIdContainer = std::vector< std::string >;
		const SeriesIdContainer & fixedSeriesUID = fixedNameGenerator->GetSeriesUIDs();
		const SeriesIdContainer & movingSeriesUID = movingNameGenerator->GetSeriesUIDs();

		std::cout << "Contains the following DICOM Series: ";
		auto seriesItr = fixedSeriesUID.begin();
		auto seriesEnd = fixedSeriesUID.end();

		while (seriesItr != seriesEnd)
		{
			std::cout << seriesItr->c_str() << std::endl << endl << endl;
			++seriesItr;
		}

		std::string fixedSeriesIdentifier;
		std::string movingSeriesIdentifier;

		// ÿ�����˶�������ص�UID
		fixedSeriesIdentifier = fixedSeriesUID.begin()->c_str();
		movingSeriesIdentifier = movingSeriesUID.begin()->c_str();

		using FileNamesContainer = std::vector< std::string >;
		FileNamesContainer fixedFileNames;
		FileNamesContainer movingFileNames;

		fixedFileNames = fixedNameGenerator->GetFileNames(fixedSeriesIdentifier);
		fixedImageReader->SetFileNames(fixedFileNames);

		movingFileNames = movingNameGenerator->GetFileNames(movingSeriesIdentifier);
		movingImageReader->SetFileNames(movingFileNames);

		try
		{
			fixedImageReader->Update();
			movingImageReader->Update();
		}
		catch (itk::ExceptionObject &ex)
		{
			std::cout << ex << std::endl;

		}

		fixedImage = fixedImageReader->GetOutput();
		movingImage = movingImageReader->GetOutput();


		// ����ͼ�����ֵ����Сֵ
		using MinMaxCalculatorType = itk::MinimumMaximumImageCalculator<MovingImageType>;
		MinMaxCalculatorType::Pointer movingCalculator = MinMaxCalculatorType::New();
		movingCalculator->SetImage(movingImage);
		movingCalculator->Compute();

		MinMaxCalculatorType::PixelType movingMaxValue = movingCalculator->GetMaximum();
		MinMaxCalculatorType::PixelType movingMinValue = movingCalculator->GetMinimum();

		using MinMaxCalculatorType = itk::MinimumMaximumImageCalculator<FixedImageType>;
		MinMaxCalculatorType::Pointer fixedCalculator = MinMaxCalculatorType::New();
		fixedCalculator->SetImage(fixedImage);
		fixedCalculator->Compute();

		MinMaxCalculatorType::PixelType fixedMaxValue = fixedCalculator->GetMaximum();
		MinMaxCalculatorType::PixelType fixedMinValue = fixedCalculator->GetMinimum();

		// ��ʾͼ����Ϣ����ȷ���Ƿ���ȷ��ȡ����
		std::cout << " ----------------- Image Information ---------------- " << endl << endl;

		// Fixed Image
		std::cout << "Fixed Image Information: " << endl;
		std::cout << std::endl;

		const FixedImageType::SpacingType& spacing = fixedImage->GetSpacing();
		std::cout << " Spacing = ";
		std::cout << spacing[0] << " " << spacing[1] << " " << spacing[2] << endl;
		const FixedImageType::PointType& origin = fixedImage->GetOrigin();
		std::cout << " Origin = ";
		std::cout << origin[0] << " " << origin[1] << " " << origin[2] << endl;
		const FixedImageType::SizeType& size = fixedImage->GetLargestPossibleRegion().GetSize();
		std::cout << " Size = ";
		std::cout << size[0] << " " << size[1] << " " << size[2] << endl;
		std::cout << " Range = ";
		std::cout << "[" << fixedMinValue << ", " << fixedMaxValue << "]" << endl << endl;

		// Moving Image
		std::cout << "Moving Image Information: " << endl;
		std::cout << std::endl;
		 
		const MovingImageType::SpacingType& spacing1 = movingImage->GetSpacing();
		std::cout << " Spacing = ";
		std::cout << spacing1[0] << " " << spacing1[1] << " " << spacing1[2] << endl;
		const MovingImageType::PointType& origin1 = movingImage->GetOrigin();
		std::cout << " Origin = ";
		std::cout << origin1[0] << " " << origin1[1] << " " << origin1[2] << endl;
		const MovingImageType::SizeType& size1 = movingImage->GetLargestPossibleRegion().GetSize();
		std::cout << " Size = ";
		std::cout << size1[0] << " " << size1[1] << " " << size1[2] << endl; 
		std::cout << " Range = ";
		std::cout << "[" << movingMinValue << ", " << movingMaxValue << "]" << endl << endl;


		std::cout << " ----------------- Image Read Finished ---------------- " << endl << endl;
		system("pause");
	}
	catch (itk::ExceptionObject &ex)
	{
		std::cout << ex << std::endl;
	}

}

void writeMHAImage(FixedImageType::Pointer& toBeWrittenImage, 
	string filePath = "./SavedData/Registered_Image.mha")
{
	using WriterType = itk::ImageFileWriter< FixedImageType >;
	WriterType::Pointer writer = WriterType::New();

	writer->SetFileName(filePath);
	writer->SetInput(toBeWrittenImage);

	try
	{
		writer->Update();
		std::cout << endl << "Finished Writing the File in:   " << filePath << endl << endl;
	}
	catch (itk::ExceptionObject & error)
	{
		std::cerr << "Error: " << error << endl << std::endl;
		system("pause");
	}

}
//
//void showFusionImage(vtkImageData* vtkFixedImage, vtkImageData* vtkFinishedImage, 
//	const float& opacity, const int& sliceIndex, string filePath = "./SavedData/Fusion_Image.mha")
//{
//	// ͼ���ں�
//	vtkSmartPointer<vtkImageBlend> imageBlend =
//		vtkSmartPointer<vtkImageBlend>::New();
//	imageBlend->AddInputData(vtkFinishedImage);
//	imageBlend->AddInputData(vtkFixedImage);
//	imageBlend->SetOpacity(0, 0.5);
//	imageBlend->SetOpacity(1, 0.5);
//	imageBlend->Update();
//
//	vtkSmartPointer<vtkImageViewer2> fusionViewer =
//		vtkSmartPointer<vtkImageViewer2>::New();
//	vtkSmartPointer<vtkRenderWindowInteractor> fusionInteractor =
//		vtkSmartPointer<vtkRenderWindowInteractor>::New();
//
//	fusionViewer->SetupInteractor(fusionInteractor);
//	fusionViewer->SetInputData(imageBlend->GetOutput());
//	fusionViewer->SetSlice(sliceIndex);
//	fusionViewer->SetSliceOrientationToXY();
//
//	// vtk to itk
//	using VTKImageToImageType = itk::VTKImageToImageFilter<FixedImageType>;
//	VTKImageToImageType::Pointer converter = VTKImageToImageType::New();
//	converter->SetInput(imageBlend->GetOutput());
//	converter->Update();
//	// ת��ΪitkImageSource��
//	
//	using WriterType = itk::ImageFileWriter< FixedImageType >;
//	WriterType::Pointer writer = WriterType::New();
//	writer->SetFileName(filePath);
//	writer->SetInput(converter->GetOutput());
//
//	try
//	{
//		writer->Update();
//		std::cout << endl << "Finished Writing the File in:   " << filePath << endl << endl;
//	}
//	catch (itk::ExceptionObject & error)
//	{
//		std::cerr << "Error: " << error << std::endl;
//	}
//
//
//	fusionViewer->Render();
//	fusionInteractor->Start();
//
//}
//

FixedImageType::Pointer imageResample(FixedImageType::Pointer& originalImage)
{
	typedef itk::ResampleImageFilter<FixedImageType, FixedImageType> ResampleImageFilterType;
	ResampleImageFilterType::Pointer resampleFilter = 
		ResampleImageFilterType::New();

	resampleFilter->SetInput(originalImage);


	using SizeType = FixedImageType::SizeType;  //����ͼ��ߴ�����
	using PixelType = FixedImageType::PixelType;  //����ͼ�����ػҶ�ֵ����
	using PoingType = FixedImageType::PointType;  //����ͼ����������λ������
	using IndexType = FixedImageType::IndexType;  //����ͼ����������ֵ����
	using DirectionType = FixedImageType::DirectionType;  //����ͼ��ķ�������
	using SpacingType = FixedImageType::SpacingType;


	SpacingType originspacing = originalImage->GetSpacing();
	SizeType size = originalImage->GetLargestPossibleRegion().GetSize();


	SizeType sizesample;
	sizesample[0] = 256;
	sizesample[1] = 256;
	sizesample[2] = size[2];

	SpacingType spacingresample;
	spacingresample[0] = size[0] * originspacing[0] / sizesample[0];
	spacingresample[1] = size[1] * originspacing[1] / sizesample[1];
	spacingresample[2] = size[2] * originspacing[2] / sizesample[2];
	//�����ز�����ͼ��Ĵ�С



	InterpolatorType::Pointer interpolator = InterpolatorType::New();
	resampleFilter->SetInterpolator(interpolator);  
	resampleFilter->SetOutputSpacing(spacingresample);
	resampleFilter->SetSize(sizesample);
	resampleFilter->SetOutputOrigin(originalImage->GetOrigin());
	resampleFilter->SetOutputDirection(originalImage->GetDirection());
	resampleFilter->SetDefaultPixelValue(-3000);

	try
	{
		resampleFilter->Update();
	}
	catch (itk::ExceptionObject & exp)
	{
		cerr << "Resample Error!" << std::endl;
		cerr << exp << std::endl;
	}



	std::cout << endl << "------------------ Resample Finished ----------------" << endl << endl;
	std::cout << "Fixed Image(After Resample) Information: " << endl;
	std::cout << std::endl;

	const FixedImageType::SpacingType& spacing_changed = resampleFilter->GetOutput()->GetSpacing();
	std::cout << " Spacing = ";
	std::cout << spacing_changed[0] << " " << spacing_changed[1] << " " << spacing_changed[2] << endl;
	const FixedImageType::PointType& origin_changed = resampleFilter->GetOutput()->GetOrigin();
	std::cout << " Origin = ";
	std::cout << origin_changed[0] << " " << origin_changed[1] << " " << origin_changed[2] << endl;
	const FixedImageType::SizeType& size_changed = resampleFilter->GetOutput()->GetLargestPossibleRegion().GetSize();
	std::cout << " Size = ";
	std::cout << size_changed[0] << " " << size_changed[1] << " " << size_changed[2] << endl;

	return resampleFilter->GetOutput();

}


// ��׼�Ż�����
RegistrationType::Pointer registrationOptimizer(FixedImageType::Pointer& fixedImage, MovingImageType::Pointer& movingImage)
{
	TransformType::Pointer    transform = TransformType::New();
	OptimizerType::Pointer    optimizer = OptimizerType::New();
	InterpolatorType::Pointer interpolator = InterpolatorType::New();
	RegistrationType::Pointer registration = RegistrationType::New();
	MetricType::Pointer       metric = MetricType::New();

	registration->SetOptimizer(optimizer);
	registration->SetTransform(transform);
	registration->SetInterpolator(interpolator);
	registration->SetMetric(metric);

	metric->SetFixedImageStandardDeviation(0.4);
	metric->SetMovingImageStandardDeviation(0.4);

	// optimizer->SetRelaxationFactor(); ��������ѧϰ��ÿ����С�ı�����Ĭ��0.5

	// ��ȡDICOM����
	readDICOMImage(fixedImage, movingImage);
	// �ز���Ϊ256*256
	FixedImageType::Pointer resampledImage = imageResample(fixedImage);


	// --------------------------- ���ݱ�׼��+��˹�˲� ----------------------------------------------

	using FixedNormalizeFilterType = itk::NormalizeImageFilter<FixedImageType, InternalImageType>;
	using MovingNormalizeFilterType = itk::NormalizeImageFilter<MovingImageType, InternalImageType>;

	FixedNormalizeFilterType::Pointer fixedNormalizer = FixedNormalizeFilterType::New();
	MovingNormalizeFilterType::Pointer movingNormalizer = MovingNormalizeFilterType::New();

	using GaussianFilterType = itk::DiscreteGaussianImageFilter<InternalImageType, InternalImageType>;

	GaussianFilterType::Pointer fixedSmoother = GaussianFilterType::New();
	GaussianFilterType::Pointer movingSmoother = GaussianFilterType::New();

	fixedSmoother->SetVariance(2.0);
	movingSmoother->SetVariance(2.0);

	//�ȱ�׼����ȥ��
	fixedNormalizer->SetInput(resampledImage);
	movingNormalizer->SetInput(movingImage);

	fixedSmoother->SetInput(fixedNormalizer->GetOutput());
	movingSmoother->SetInput(movingNormalizer->GetOutput());

	InternalImageType::Pointer fixedImageInter = fixedSmoother->GetOutput();
	InternalImageType::Pointer movingImageInter = movingSmoother->GetOutput();

	//fixedImageInter->Update();
	//movingImageInter->Update();



	// --------------------------- 3D���Ա任���ĳ�ʼ�� ------------------------------------
	using TransformInitializerType =
		itk::CenteredTransformInitializer<TransformType, FixedImageType, MovingImageType>;
	TransformInitializerType::Pointer initializer = TransformInitializerType::New();

	initializer->SetTransform(transform);
	initializer->SetFixedImage(fixedSmoother->GetOutput());
	initializer->SetMovingImage(movingSmoother->GetOutput());
	//initializer->MomentsOn(); // �����ƶ�����Ϊ���ģ� ����ʾ˵���س�����Χ
	// ��������ͨ���������ڶ�ģ̬��׼
	initializer->GeometryOn(); // Fixed Image���������� 
	initializer->InitializeTransform();

	using VersorType = TransformType::VersorType;
	using VectorType = VersorType::VectorType;
	VersorType  rotation;
	VectorType  axis;
	axis[0] = 0.0;
	axis[1] = 0.0;
	axis[2] = 1.0;
	constexpr double angle = 0;
	rotation.Set(axis, angle);
	transform->SetRotation(rotation);


	// ��ʾ Fixed Image ��ת��������
	RegistrationType::ParametersType center = transform->GetFixedParameters();
	std::cout << endl << "Center of Fixed Image is: " << "[" << center[0] << ", " 
		<< center[1] << ", " << center[2] << "]" << endl;

	FixedImageType::IndexType pixelIndex;
	FixedImageType::PointType point;
	point[0] = center[0];
	point[1] = center[1];
	point[2] = center[2];

	bool isInside = fixedSmoother->GetOutput()->TransformPhysicalPointToIndex(point, pixelIndex);
	if (isInside)
	{
		std::cout << "the center of fixed image is: " << "[" << pixelIndex[0] << ", "
			<< pixelIndex[1] << ", " << pixelIndex[2] << "]" << endl << endl;
	}
	else
	{
		std::cout << "the point is outside of image !" << endl << endl;
	}


	// --------------------------- ���ò��� ----------------------------------------
	registration->SetFixedImage(fixedSmoother->GetOutput());
	registration->SetMovingImage(movingSmoother->GetOutput());

	fixedNormalizer->Update();
	FixedImageType::RegionType fixedImageRegion = fixedNormalizer->GetOutput()->GetBufferedRegion();
	registration->SetFixedImageRegion(fixedImageRegion);


	//transform->GetFixedParameters()��GetParameters()�����߾�����itk::TransformBase::ParametersType���͵Ķ���
	// �������Ż���ɺ���ʹ��registration->GetLastTransformParameters()�õ��Ż������ά����
	// �˴�ֻ��Ҫ������ά����������Ҫ������ת���ģ���Ϊ��ʼʱ��������registration->SetTransform(transform);
	// SetInitialTransformParametersֻ��������ά������SetTransform��Ҫ���������ı任��6+3��
	registration->SetInitialTransformParameters(transform->GetParameters());


	// �����Ż����и�������Ȩ��
	using OptimizerScalesType = OptimizerType::ScalesType;
	OptimizerScalesType optimizerScales(transform->GetNumberOfParameters());
	const double translationScale = 1.0 / 1000.0;
	/*versor�У��Ƕ����Ի�����Ϊ��λ�ģ�����ֵһ�㶼��С��ƽ������mmΪ��λ�ģ�����ֵ��ԽǶ�ֵ�ϴ�
	  �����Ҫһ�����ű������ӣ���ƽ�Ƶĵ�λ��С1000�����������Ż�ʱ���в����Ķ�̬��Χ�ͽ�Ϊһ�£����������Ż�*/

	optimizerScales[0] = 1.0;
	optimizerScales[1] = 1.0;
	optimizerScales[2] = 1.0;
	optimizerScales[3] = translationScale;
	optimizerScales[4] = translationScale;
	optimizerScales[5] = translationScale;

	optimizer->SetScales(optimizerScales);
	optimizer->SetNumberOfIterations(50);
	optimizer->SetMaximumStepLength(4);
	optimizer->SetMinimumStepLength(0.00001);
	optimizer->MaximizeOn();

	// Ĭ��ֵΪ0.5������������̫�����У���ʱ�����Ĵ��ڻ�ʹ��ѧϰ����ǰ��С��������ô�һЩ
	optimizer->SetRelaxationFactor(0.8);

	// ����metric����
	const unsigned int numberOfPixels = fixedImageRegion.GetNumberOfPixels();

	//const auto numberOfSamples = static_cast<unsigned int>(numberOfPixels * 0.01);  //���ó�0.02�ͻ��ú���

	const auto numberOfSamples = static_cast<unsigned int>(2000); 
	/* numberOfSamples��ͼ��ߴ��С�йأ�ͼ��ߴ�Խ�����õ�����Ӧ��Խ��
	   ���ز���Ϊ256*256�Ļ�ֻ������2000���ɻ�ýϺõ��������ٶȣ���Ϊ512*512�ߴ���Ҫ����3000*/

	metric->SetNumberOfSpatialSamples(numberOfSamples);
	
	// For consistent results when regression testing.
	//metric->ReinitializeSeed(121212);
	metric->ReinitializeSeed(1000); 

	CommandIterationUpdate::Pointer observer = CommandIterationUpdate::New();
	optimizer->AddObserver(itk::IterationEvent(), observer);

	try
	{
		cout << endl << " ----------------- Registration Start ---------------- " << endl << endl;
		registration->Update();
		std::cout << endl <<  "Optimizer stop condition: " << registration->GetOptimizer()->GetStopConditionDescription()
			<< std::endl << endl;
	}
	catch (itk::ExceptionObject & err)
	{
		std::cout << "ExceptionObject caught !" << std::endl;
		std::cout << err << std::endl;
		system("pause");

	}

	system("pause");

	return registration;
}



int main(int argc, char * argv[])
{
	
	FixedImageType::Pointer fixedImage;
	MovingImageType::Pointer movingImage;

	//readDICOMImage(fixedImage, movingImage);
	//imageResample(fixedImage);


	RegistrationType::Pointer registration = registrationOptimizer(fixedImage, movingImage);




	//transform->SetTranslation(movingimage - fixedimage);
	//transform->SetCenter(fixedimage);
	//// Note that large values of the learning rate will make the optimizer
	//// unstable. Small values, on the other hand, may result in the optimizer
	//// needing too many iterations in order to walk to the extrema of the cost
	//// function. The easy way of fine tuning this parameter is to start with
	//// small values, probably in the range of {5.0, 10.0}. Once the other
	//// registration parameters have been tuned for producing convergence, you
	//// may want to revisit the learning rate and start increasing its value until
	//// you observe that the optimization becomes unstable.  The ideal value for
	//// this parameter is the one that results in a minimum number of iterations
	//// while still keeping a stable path on the parametric space of the
	//// optimization. Keep in mind that this parameter is a multiplicative factor
	//// applied on the gradient of the Metric. Therefore, its effect on the
	//// optimizer step length is proportional to the Metric values themselves.
	//// Metrics with large values will require you to use smaller values for the
	//// learning rate in order to maintain a similar optimizer behavior.

	////  We should now define the number of spatial samples to be considered in
	////  the metric computation. Note that we were forced to postpone this setting
	////  until we had done the preprocessing of the images because the number of
	////  samples is usually defined as a fraction of the total number of pixels in
	////  the fixed image.
	////
	////  The number of spatial samples can usually be as low as $1\%$ of the total
	////  number of pixels in the fixed image. Increasing the number of samples
	////  improves the smoothness of the metric from one iteration to another and
	////  therefore helps when this metric is used in conjunction with optimizers
	////  that rely of the continuity of the metric values. The trade-off, of
	////  course, is that a larger number of samples result in longer computation
	////  times per every evaluation of the metric.
	////
	////  It has been demonstrated empirically that the number of samples is not a
	////  critical parameter for the registration process. When you start fine
	////  tuning your own registration process, you should start using high values
	////  of number of samples, for example in the range of 20% to 50% of the 
	////  number of pixels in the fixed image. Once you have succeeded to register
	////  your images you can then reduce the number of samples progressively until
	////  you find a good compromise on the time it takes to compute one evaluation
	////  of the Metric. Note that it is not useful to have very fast evaluations
	////  of the Metric if the noise in their values results in more iterations
	////  being required by the optimizer to converge.
	////  behavior of the metric values as the iterations progress.

	// ----------------------------------  ����Transform  ---------------------------------------
	// C3L-03728 lr = 4 
	/*double parameters[6] = { 0.02883648, -0.02300000, -0.077098042, -9.59033557, -46.200972, 22.4431265 };
	double fixedparameters[3] = { -0.244204, 8.05884, 42.7288 };*/
	/// resample 256*256
	/*double parameters[6] = { 0.02713198, -0.0254727, -0.0755447985, -9.68685, -46.128, 22.4623 };
	double fixedparameters[3] = { -0.488345, 7.81805, 42.7691 };*/

	// C3L-02465 ��һ�� lr = 8
	//double parameters[6] = { -0.012151522, -0.00709238, 0.00801453, -9.22467169, -41.20525855, 39.413676 };
	//double fixedparameters[3] = { -0.244204, -0.239321, -1.75403 };

	// C3L-02465 ��һ�� lr = 8
	//double parameters[6] = { -0.01960266, 0.03580786, 0.0608723, -1.5327088, -9.564191, 89.082410 };
	//double fixedparameters[3] = { 1.3558, -4.93107, -3.58848 };
	/// resample 256*256
	double parameters[6] = { -0.021655, 0.036076, 0.059328, -1.495600, -9.44450, 88.80189};
	double fixedparameters[3] = { 1.11165, -5.16191, -3.509 };


	using ParametersType = RegistrationType::ParametersType;

	//auto f = registration->GetModifiableTransform();
/*
	auto finalParameters = registration->GetTransform();
*/

	ParametersType finalParameters;
	finalParameters.SetData(parameters, 6);

	ParametersType fixedParameters;
	fixedParameters.SetData(fixedparameters, 3);

	// registration����itk::TransformBase::ParametersType���͵Ķ���
	// ������������ <��׼�����Ĳ�������> RegistrationType::ParametersType
	// �˲���ֻ����ά������������fixed parameters����ת��������

	//ParametersType finalParameters = registration->GetLastTransformParameters();
	
	//double finalParameters0 = finalParameters[0];
	//double finalParameters1 = finalParameters[1];
	//double finalParameters2 = finalParameters[2];
	//double finalParameters3 = finalParameters[3];
	//double finalParameters4 = finalParameters[4];
	//double finalParameters5 = finalParameters[5];
	//double fixedParameters0 = fixedParameters[0];
	//double fixedParameters1 = fixedParameters[1];
	//double fixedParameters2 = fixedParameters[2];
	//unsigned int numberOfIterations = optimizer->GetCurrentIteration();
	//double bestValue = optimizer->GetValue();
	//// Print out results
	//std::cout << std::endl;
	//std::cout << "Result = " << std::endl;
	//std::cout << " Translation 0 = " << finalParameters0 << std::endl;
	//std::cout << " Translation 1 = " << finalParameters1 << std::endl;
	//std::cout << " Translation 2 = " << finalParameters2 << std::endl;
	//std::cout << " Translation 3 = " << finalParameters3 << std::endl;
	//std::cout << " Translation 4 = " << finalParameters4 << std::endl;
	//std::cout << " Translation 5 = " << finalParameters5 << std::endl;
	//std::cout << " Fixed Translation 0 = " << fixedParameters0 << std::endl;
	//std::cout << " Fixed Translation 1 = " << fixedParameters1 << std::endl;
	//std::cout << " Fixed Translation 2 = " << fixedParameters2 << std::endl;
	//std::cout << " Iterations    = " << numberOfIterations << std::endl;
	//std::cout << " Metric value  = " << bestValue << std::endl;
	//std::cout << " Numb. Samples = " << numberOfSamples << std::endl;


	// itkVersorRigid3DTransform
	TransformType::Pointer finalTransform = TransformType::New();

	/* 
	itkVersorRigid3DTransform��Ķ�������������ɣ�
		(1) optimizable parameters��  ���Ż��Ĳ�����ָ��תƽ����ά������
			��ά������������ת�Ƕ���ƽ�ƾ��룬��SetParameters()����
		(2) fixed parameters��  �̶��Ĳ�����ָ��ת���ģ�
			��ά����������moving image����ת�������꣬��SetFixedParameters()����
	*/

	//transform->GetFixedParameters()��GetParameters()�����߾�����itk::TransformBase::ParametersType���͵Ķ���
	finalTransform->SetParameters(finalParameters);
	finalTransform->SetFixedParameters(fixedParameters); //transform->GetFixedParameters()



	// ----------------------------------  ��moving image������׼ ---------------------------------------
	// ��ȡͼ��
	readDICOMImage(fixedImage, movingImage);

	using ResampleFilterType = itk::ResampleImageFilter<MovingImageType, FixedImageType>;
	ResampleFilterType::Pointer resample = ResampleFilterType::New();

	resample->SetTransform(finalTransform);
	//resample->SetTransform(registration->GetOutput()->Get());
	resample->SetInput(movingImage);

	resample->SetSize(fixedImage->GetLargestPossibleRegion().GetSize());
	resample->SetOutputOrigin(fixedImage->GetOrigin());
	resample->SetOutputSpacing(fixedImage->GetSpacing());
	// direction: A matrix of direction cosines. ��ʾͼ��ֲ�����ϵ�ķ������3Dͼ��Ϊ4��4��Matrix
	// �˴���direction����ָ����DICOM�еķ��򣬼�ͼ���һ�����һ���벡�˱�׼���������cosineֵ
	// ��һ����ά����������1/0/0/0/0.9862/-0.1650������ͼ���з���Ϊ��׼x�����з������׼y�����cosineΪ0.9862
	resample->SetOutputDirection(fixedImage->GetDirection());
	resample->SetDefaultPixelValue(100);

	try
	{
		resample->Update();
	}
	catch (itk::ExceptionObject & error)
	{
		std::cerr << "Error: " << error << std::endl;
		return EXIT_FAILURE;
	}

	FixedImageType::Pointer finishedImage = resample->GetOutput();


	// ----------------------------------  ��ʾ�����׼ͼ�����Ϣ ----------------------------------  
	
	std::cout << endl << "Registration Finished Image Information: " << endl;
	std::cout << std::endl;

	const FixedImageType::SpacingType& spacing_finished = finishedImage->GetSpacing();
	std::cout << " Spacing = ";
	std::cout << spacing_finished[0] << " " << spacing_finished[1] << " " << spacing_finished[2] << endl;
	const FixedImageType::PointType& origin_finished = finishedImage->GetOrigin();
	std::cout << " Origin = ";
	std::cout << origin_finished[0] << " " << origin_finished[1] << " " << origin_finished[2] << endl;
	const FixedImageType::SizeType& size_finished = finishedImage->GetLargestPossibleRegion().GetSize();
	std::cout << " Size = ";
	std::cout << size_finished[0] << " " << size_finished[1] << " " << size_finished[2] << endl << endl;


	// -------------------------  ����ͼ��Ϊ.mha�ļ����Թ۲���׼��� -------------------------  
	//writeMHAImage(finishedImage, "./SavedData/patient1-resample.mha");
	//system("pause");



	//// ---------------------------------- ת����vtk����ʾ -----------------------------------------
	//using FilterType = itk::ImageToVTKImageFilter<FixedImageType>;
	//FilterType::Pointer filter1 = FilterType::New();
	//filter1->SetInput(finishedImage);

	//try
	//{
	//	filter1->Update();
	//}
	//catch (itk::ExceptionObject & error)
	//{
	//	std::cerr << "Error: " << error << std::endl;
	//	return EXIT_FAILURE;
	//}

	//vtkImageData * vtkFinishedImage = filter1->GetOutput();

	////vtkSmartPointer<vtkImageViewer2> imageViewer1 = 
	////	vtkSmartPointer<vtkImageViewer2>::New();
	////vtkSmartPointer<vtkRenderWindowInteractor> interactor1 =
	////	vtkSmartPointer<vtkRenderWindowInteractor>::New();
	////imageViewer1->SetupInteractor(interactor1);
	////imageViewer1->SetInputData(vtkFinishedImage);
	////imageViewer1->SetSlice(16);
	////imageViewer1->SetSliceOrientationToXY();


	//FilterType::Pointer filter2 = FilterType::New();
	//filter2->SetInput(fixedImage);

	//try
	//{
	//	filter2->Update();
	//}
	//catch (itk::ExceptionObject & error)
	//{
	//	std::cerr << "Error: " << error << std::endl;
	//	return EXIT_FAILURE;
	//}

	//vtkImageData * vtkFixedImage = filter2->GetOutput();

	////vtkSmartPointer<vtkImageViewer2> imageViewer2 =
	////	vtkSmartPointer<vtkImageViewer2>::New();
	////vtkSmartPointer<vtkRenderWindowInteractor> interactor2 =
	////	vtkSmartPointer<vtkRenderWindowInteractor>::New();
	////imageViewer2->SetupInteractor(interactor2);
	////imageViewer2->SetInputData(vtkFixedImage);
	////imageViewer2->SetSlice(16);
	////imageViewer2->SetSliceOrientationToXY();


	////imageViewer1->Render();
	////interactor1->Start();
	////imageViewer2->Render();
	////interactor2->Start();


	//// ---------------------------------- ͼ���ںϲ�����.mha���� -------------------------------
	//float opacity = 0.5;
	//int sliceIndex = 31;
	//string path = "./SavedData/patient1-resample.mha";
	//	
	//showFusionImage(vtkFixedImage, vtkFinishedImage, opacity, sliceIndex, path);

	//system("pause");

	//using OutputPixelType = unsigned char;
	//using OutputImageType = itk::Image<OutputPixelType, Dimension>;
	//using CastFilterType = itk::CastImageFilter<FixedImageType, OutputImageType>;
	//using WriterType = itk::ImageFileWriter<OutputImageType>;

	//WriterType::Pointer     writer = WriterType::New();
	//CastFilterType::Pointer caster = CastFilterType::New();

	//// ������׼���ͼ��
	//writer->SetFileName(outputImageFile);
	//caster->SetInput(resample->GetOutput());
	//writer->SetInput(caster->GetOutput());
	//writer->Update();

	//// Generate checkerboards before and after registration 
	//// �������̸�
	//using CheckerBoardFilterType = itk::CheckerBoardImageFilter<FixedImageType>;

	//CheckerBoardFilterType::Pointer checker = CheckerBoardFilterType::New();

	//checker->SetInput1(fixedImage);
	//checker->SetInput2(resample->GetOutput());

	//caster->SetInput(checker->GetOutput());
	//writer->SetInput(caster->GetOutput());


	std::cout << endl << " ----------------- END -----------------" << endl;
	system("pause");

	return EXIT_SUCCESS;
}