
#include "GenericReconCartesianGrappaGadget.h"
#include "mri_core_grappa.h"

/*
    The input is IsmrmrdReconData and output is single 2D or 3D ISMRMRD images

    If required, the gfactor map can be sent out

    If the  number of required destination channel is 1, the GrappaONE recon will be performed

    The image number computation logic is implemented in compute_image_number function, which can be overloaded
*/

namespace Gadgetron {

    GenericReconCartesianGrappaGadget::GenericReconCartesianGrappaGadget() : BaseClass()
    {
    }

    GenericReconCartesianGrappaGadget::~GenericReconCartesianGrappaGadget()
    {
    }

    int GenericReconCartesianGrappaGadget::process_config(ACE_Message_Block* mb)
    {
        GADGET_CHECK_RETURN(BaseClass::process_config(mb) == GADGET_OK, GADGET_FAIL);

        // -------------------------------------------------

        ISMRMRD::IsmrmrdHeader h;
        try
        {
            deserialize(mb->rd_ptr(), h);
        }
        catch (...)
        {
            GDEBUG("Error parsing ISMRMRD Header");
        }

        size_t NE = h.encoding.size();
        num_encoding_spaces_ = NE;
        GDEBUG_CONDITION_STREAM(verbose.value(), "Number of encoding spaces: " << NE);

        recon_obj_.resize(NE);

        return GADGET_OK;
    }

    int GenericReconCartesianGrappaGadget::process(Gadgetron::GadgetContainerMessage< IsmrmrdReconData >* m1)
    {
        if (perform_timing.value()) { gt_timer_local_.start("GenericReconCartesianGrappaGadget::process"); }

        process_called_times_++;

        IsmrmrdReconData* recon_bit_ = m1->getObjectPtr();
        if (recon_bit_->rbit_.size() > num_encoding_spaces_)
        {
            GWARN_STREAM("Incoming recon_bit has more encoding spaces than the protocol : " << recon_bit_->rbit_.size() << " instead of " << num_encoding_spaces_);
        }

        // for every encoding space
        for (size_t e = 0; e < recon_bit_->rbit_.size(); e++)
        {
            std::stringstream os;
            os << "_encoding_" << e;

            GDEBUG_CONDITION_STREAM(verbose.value(), "Calling " << process_called_times_ << " , encoding space : " << e);
            GDEBUG_CONDITION_STREAM(verbose.value(), "======================================================================");

            // ---------------------------------------------------------------
            // export incoming data

            /*if (!debug_folder_full_path_.empty())
            {
                gt_exporter_.exportArrayComplex(recon_bit_->rbit_[e].data_.data_, debug_folder_full_path_ + "data" + os.str());
            }

            if (!debug_folder_full_path_.empty() && recon_bit_->rbit_[e].data_.trajectory_)
            {
                if (recon_bit_->rbit_[e].ref_->trajectory_->get_number_of_elements() > 0)
                {
                    gt_exporter_.exportArray(*(recon_bit_->rbit_[e].data_.trajectory_), debug_folder_full_path_ + "data_traj" + os.str());
                }
            }*/

            // ---------------------------------------------------------------

            if (recon_bit_->rbit_[e].ref_)
            {
                /*if (!debug_folder_full_path_.empty())
                {
                    gt_exporter_.exportArrayComplex(recon_bit_->rbit_[e].ref_->data_, debug_folder_full_path_ + "ref" + os.str());
                }

                if (!debug_folder_full_path_.empty() && recon_bit_->rbit_[e].ref_->trajectory_)
                {
                    if (recon_bit_->rbit_[e].ref_->trajectory_->get_number_of_elements() > 0)
                    {
                        gt_exporter_.exportArray(*(recon_bit_->rbit_[e].ref_->trajectory_), debug_folder_full_path_ + "ref_traj" + os.str());
                    }
                }*/

                // ---------------------------------------------------------------

                // after this step, the recon_obj_[e].ref_calib_ and recon_obj_[e].ref_coil_map_ are set

                if (perform_timing.value()) { gt_timer_.start("GenericReconCartesianGrappaGadget::make_ref_coil_map"); }
                this->make_ref_coil_map(*recon_bit_->rbit_[e].ref_,*recon_bit_->rbit_[e].data_.data_.get_dimensions(), recon_obj_[e].ref_calib_, recon_obj_[e].ref_coil_map_, e);
                if (perform_timing.value()) { gt_timer_.stop(); }

                // ----------------------------------------------------------
                // export prepared ref for calibration and coil map
                /*if (!debug_folder_full_path_.empty())
                {
                    this->gt_exporter_.exportArrayComplex(recon_obj_[e].ref_calib_, debug_folder_full_path_ + "ref_calib" + os.str());
                }

                if (!debug_folder_full_path_.empty())
                {
                    this->gt_exporter_.exportArrayComplex(recon_obj_[e].ref_coil_map_, debug_folder_full_path_ + "ref_coil_map" + os.str());
                }*/

                // ---------------------------------------------------------------

                // after this step, coil map is computed and stored in recon_obj_[e].coil_map_
                if (perform_timing.value()) { gt_timer_.start("GenericReconCartesianGrappaGadget::perform_coil_map_estimation"); }
                this->perform_coil_map_estimation(recon_obj_[e].ref_coil_map_, recon_obj_[e].coil_map_, e);
                if (perform_timing.value()) { gt_timer_.stop(); }

                // ---------------------------------------------------------------

                // after this step, recon_obj_[e].kernel_, recon_obj_[e].kernelIm_, recon_obj_[e].unmixing_coeff_ are filled
                // gfactor is computed too
                if (perform_timing.value()) { gt_timer_.start("GenericReconCartesianGrappaGadget::perform_calib"); }
                this->perform_calib(recon_bit_->rbit_[e], recon_obj_[e], e);
                if (perform_timing.value()) { gt_timer_.stop(); }

                // ---------------------------------------------------------------

                recon_bit_->rbit_[e].ref_ = boost::none;
            }

            if (recon_bit_->rbit_[e].data_.data_.get_number_of_elements() > 0)
            {
                /*if (!debug_folder_full_path_.empty())
                {
                    gt_exporter_.exportArrayComplex(recon_bit_->rbit_[e].data_.data_, debug_folder_full_path_ + "data_before_unwrapping" + os.str());
                }

                if (!debug_folder_full_path_.empty() && recon_bit_->rbit_[e].data_.trajectory_)
                {
                    if (recon_bit_->rbit_[e].data_.trajectory_->get_number_of_elements() > 0)
                    {
                        gt_exporter_.exportArray(*(recon_bit_->rbit_[e].data_.trajectory_), debug_folder_full_path_ + "data_before_unwrapping_traj" + os.str());
                    }
                }*/

                // ---------------------------------------------------------------

                if (perform_timing.value()) { gt_timer_.start("GenericReconCartesianGrappaGadget::perform_unwrapping"); }
                this->perform_unwrapping(recon_bit_->rbit_[e], recon_obj_[e], e);
                if (perform_timing.value()) { gt_timer_.stop(); }

                // ---------------------------------------------------------------

                if (perform_timing.value()) { gt_timer_.start("GenericReconCartesianGrappaGadget::compute_image_header"); }
                this->compute_image_header(recon_bit_->rbit_[e], recon_obj_[e].recon_res_, e);
                if (perform_timing.value()) { gt_timer_.stop(); }

                // ---------------------------------------------------------------

                if (perform_timing.value()) { gt_timer_.start("GenericReconCartesianGrappaGadget::send_out_image_array"); }
                this->send_out_image_array(recon_bit_->rbit_[e], recon_obj_[e].recon_res_, e, image_series.value() + ((int)e + 1), GADGETRON_IMAGE_REGULAR);
                if (perform_timing.value()) { gt_timer_.stop(); }

                if (send_out_gfactor.value() && recon_obj_[e].gfactor_.get_number_of_elements()>0 && (acceFactorE1_[e] * acceFactorE2_[e]>1))
                {
                    IsmrmrdImageArray res;
                    Gadgetron::real_to_complex(recon_obj_[e].gfactor_, res.data_);
                    res.headers_ = recon_obj_[e].recon_res_.headers_;
                    res.meta_ = recon_obj_[e].recon_res_.meta_;

                    if (perform_timing.value()) { gt_timer_.start("GenericReconCartesianGrappaGadget::send_out_image_array"); }
                    this->send_out_image_array(recon_bit_->rbit_[e], res, e, image_series.value() + 10 * ((int)e + 1), GADGETRON_IMAGE_GFACTOR);
                    if (perform_timing.value()) { gt_timer_.stop(); }
                }
            }

            recon_obj_[e].recon_res_.data_.clear();
            recon_obj_[e].gfactor_.clear();
            recon_obj_[e].recon_res_.headers_.clear();
            recon_obj_[e].recon_res_.meta_.clear();
        }

        m1->release();

        if (perform_timing.value()) { gt_timer_local_.stop(); }

        return GADGET_OK;
    }

    void GenericReconCartesianGrappaGadget::perform_calib(IsmrmrdReconBit& recon_bit, ReconObjType& recon_obj, size_t e)
    {
        try
        {
            size_t RO = recon_bit.data_.data_.get_size(0);
            size_t E1 = recon_bit.data_.data_.get_size(1);
            size_t E2 = recon_bit.data_.data_.get_size(2);

            hoNDArray< std::complex<float> >& src = recon_obj.ref_calib_;
            hoNDArray< std::complex<float> >& dst = recon_obj.ref_calib_;

            size_t ref_RO = src.get_size(0);
            size_t ref_E1 = src.get_size(1);
            size_t ref_E2 = src.get_size(2);
            size_t srcCHA = src.get_size(3);
            size_t ref_N = src.get_size(4);
            size_t ref_S = src.get_size(5);
            size_t ref_SLC = src.get_size(6);

            size_t dstCHA = dst.get_size(3);

            recon_obj.unmixing_coeff_.create(RO, E1, E2, srcCHA, ref_N, ref_S, ref_SLC);
            recon_obj.gfactor_.create(RO, E1, E2, 1, ref_N, ref_S, ref_SLC);

            Gadgetron::clear(recon_obj.unmixing_coeff_);
            Gadgetron::clear(recon_obj.gfactor_);

            if (acceFactorE1_[e] <= 1 && acceFactorE2_[e] <= 1)
            {
                Gadgetron::conjugate(recon_obj.coil_map_, recon_obj.unmixing_coeff_);
            }
            else
            {
                // allocate buffer for kernels
                size_t kRO = grappa_kSize_RO.value();
                size_t kNE1 = grappa_kSize_E1.value();
                size_t kNE2 = grappa_kSize_E2.value();

                size_t convKRO(1), convKE1(1), convKE2(1);

                if (E2 > 1)
                {
                    std::vector<int> kE1, oE1;
                    std::vector<int> kE2, oE2;
                    bool fitItself = true;
                    grappa3d_kerPattern(kE1, oE1, kE2, oE2, convKRO, convKE1, convKE2, (size_t)acceFactorE1_[e], (size_t)acceFactorE2_[e], kRO, kNE1, kNE2, fitItself);
                }
                else
                {
                    std::vector<int> kE1, oE1;
                    bool fitItself = true;
                    Gadgetron::grappa2d_kerPattern(kE1, oE1, convKRO, convKE1, (size_t)acceFactorE1_[e], kRO, kNE1, fitItself);
                    recon_obj.kernelIm_.create(RO, E1, 1, srcCHA, dstCHA, ref_N, ref_S, ref_SLC);
                }

                recon_obj.kernel_.create(convKRO, convKE1, convKE2, srcCHA, dstCHA, ref_N, ref_S, ref_SLC);

                Gadgetron::clear(recon_obj.kernel_);
                Gadgetron::clear(recon_obj.kernelIm_);

                long long num = ref_N*ref_S*ref_SLC;

                long long ii;

#pragma omp parallel for default(none) private(ii) shared(src, dst, recon_obj, e, num, ref_N, ref_S, ref_RO, ref_E1, ref_E2, RO, E1, E2, dstCHA, srcCHA, convKRO, convKE1, convKE2, kRO, kNE1, kNE2) if(num>1)
                for (ii = 0; ii < num; ii++)
                {
                    size_t slc = ii / (ref_N*ref_S);
                    size_t s = (ii - slc*ref_N*ref_S) / (ref_N);
                    size_t n = ii - slc*ref_N*ref_S - s*ref_N;

                    std::stringstream os;
                    os << "n" << n << "_s" << s << "_slc" << slc << "_encoding_" << e;
                    std::string suffix = os.str();

                    std::complex<float>* pSrc = &(src(0, 0, 0, 0, n, s, slc));
                    hoNDArray< std::complex<float> > ref_src(ref_RO, ref_E1, ref_E2, srcCHA, pSrc);

                    std::complex<float>* pDst = &(dst(0, 0, 0, 0, n, s, slc));
                    hoNDArray< std::complex<float> > ref_dst(ref_RO, ref_E1, ref_E2, dstCHA, pDst);

                    // -----------------------------------

                    if (E2 > 1)
                    {
                        hoNDArray< std::complex<float> > ker(convKRO, convKE1, convKE2, srcCHA, dstCHA, &(recon_obj.kernel_(0, 0, 0, 0, 0, n, s, slc)));
                        Gadgetron::grappa3d_calib_convolution_kernel(ref_src, ref_dst, (size_t)acceFactorE1_[e], (size_t)acceFactorE2_[e], grappa_reg_lamda.value(), grappa_calib_over_determine_ratio.value(), kRO, kNE1, kNE2, ker);

                        //if (!debug_folder_full_path_.empty())
                        //{
                        //    gt_exporter_.exportArrayComplex(ker, debug_folder_full_path_ + "convKer3D_" + suffix);
                        //}

                        hoNDArray< std::complex<float> > coilMap(RO, E1, E2, dstCHA, &(recon_obj.coil_map_(0, 0, 0, 0, n, s, slc)));
                        hoNDArray< std::complex<float> > unmixC(RO, E1, E2, srcCHA, &(recon_obj.unmixing_coeff_(0, 0, 0, 0, n, s, slc)));
                        hoNDArray<float> gFactor(RO, E1, E2, 1, &(recon_obj.gfactor_(0, 0, 0, 0, n, s, slc)));
                        Gadgetron::grappa3d_unmixing_coeff(ker, coilMap, (size_t)acceFactorE1_[e], (size_t)acceFactorE2_[e], unmixC, gFactor);

                        //if (!debug_folder_full_path_.empty())
                        //{
                        //    gt_exporter_.exportArrayComplex(unmixC, debug_folder_full_path_ + "unmixC_3D_" + suffix);
                        //}

                        //if (!debug_folder_full_path_.empty())
                        //{
                        //    gt_exporter_.exportArray(gFactor, debug_folder_full_path_ + "gFactor_3D_" + suffix);
                        //}
                    }
                    else
                    {
                        hoNDArray< std::complex<float> > acsSrc(ref_RO, ref_E1, srcCHA, const_cast< std::complex<float>*>(ref_src.begin()));
                        hoNDArray< std::complex<float> > acsDst(ref_RO, ref_E1, dstCHA, const_cast< std::complex<float>*>(ref_dst.begin()));

                        hoNDArray< std::complex<float> > convKer(convKRO, convKE1, srcCHA, dstCHA, &(recon_obj.kernel_(0, 0, 0, 0, 0, n, s, slc)));
                        hoNDArray< std::complex<float> > kIm(RO, E1, srcCHA, dstCHA, &(recon_obj.kernelIm_(0, 0, 0, 0, 0, n, s, slc)));

                        Gadgetron::grappa2d_calib_convolution_kernel(acsSrc, acsDst, (size_t)acceFactorE1_[e], grappa_reg_lamda.value(), kRO, kNE1, convKer);
                        Gadgetron::grappa2d_image_domain_kernel(convKer, RO, E1, kIm);

                        //if (!debug_folder_full_path_.empty())
                        //{
                        //    gt_exporter_.exportArrayComplex(convKer, debug_folder_full_path_ + "convKer_" + suffix);
                        //}

                        //if (!debug_folder_full_path_.empty())
                        //{
                        //    gt_exporter_.exportArrayComplex(kIm, debug_folder_full_path_ + "kIm_" + suffix);
                        //}

                        hoNDArray< std::complex<float> > coilMap(RO, E1, dstCHA, &(recon_obj.coil_map_(0, 0, 0, 0, n, s, slc)));
                        hoNDArray< std::complex<float> > unmixC(RO, E1, srcCHA, &(recon_obj.unmixing_coeff_(0, 0, 0, 0, n, s, slc)));
                        hoNDArray<float> gFactor;

                        Gadgetron::grappa2d_unmixing_coeff(kIm, coilMap, (size_t)acceFactorE1_[e], unmixC, gFactor);
                        memcpy(&(recon_obj.gfactor_(0, 0, 0, 0, n, s, slc)), gFactor.begin(), gFactor.get_number_of_bytes());

                        //if (!debug_folder_full_path_.empty())
                        //{
                        //    gt_exporter_.exportArrayComplex(unmixC, debug_folder_full_path_ + "unmixC_" + suffix);
                        //}

                        //if (!debug_folder_full_path_.empty())
                        //{
                        //    gt_exporter_.exportArray(gFactor, debug_folder_full_path_ + "gFactor_" + suffix);
                        //}
                    }

                    // -----------------------------------
                }
            }
        }
        catch (...)
        {
            GADGET_THROW("Errors happened in GenericReconCartesianGrappaGadget::perform_calib(...) ... ");
        }
    }

    void GenericReconCartesianGrappaGadget::perform_unwrapping(IsmrmrdReconBit& recon_bit, ReconObjType& recon_obj, size_t e)
    {
        try
        {
            typedef std::complex<float> T;

            size_t RO = recon_bit.data_.data_.get_size(0);
            size_t E1 = recon_bit.data_.data_.get_size(1);
            size_t E2 = recon_bit.data_.data_.get_size(2);
            size_t dstCHA = recon_bit.data_.data_.get_size(3);
            size_t N = recon_bit.data_.data_.get_size(4);
            size_t S = recon_bit.data_.data_.get_size(5);
            size_t SLC = recon_bit.data_.data_.get_size(6);

            hoNDArray< std::complex<float> >& src = recon_obj.ref_calib_;
            hoNDArray< std::complex<float> >& dst = recon_obj.ref_calib_;

            size_t ref_RO = src.get_size(0);
            size_t ref_E1 = src.get_size(1);
            size_t ref_E2 = src.get_size(2);
            size_t srcCHA = src.get_size(3);
            size_t ref_N = src.get_size(4);
            size_t ref_S = src.get_size(5);
            size_t ref_SLC = src.get_size(6);

            size_t convkRO = recon_obj.kernel_.get_size(0);
            size_t convkE1 = recon_obj.kernel_.get_size(1);
            size_t convkE2 = recon_obj.kernel_.get_size(2);

            recon_obj.recon_res_.data_.create(RO, E1, E2, 1, N, S, SLC);

            //if (!debug_folder_full_path_.empty())
            //{
            //    std::stringstream os;
            //    os << "encoding_" << e;
            //    std::string suffix = os.str();
            //    gt_exporter_.exportArrayComplex(recon_bit.data_.data_, debug_folder_full_path_ + "data_src_" + suffix);
            //}

            // compute aliased images
            data_recon_buf_.create(RO, E1, E2, dstCHA, N, S, SLC);

            if (E2>1)
            {
                Gadgetron::hoNDFFT<float>::instance()->ifft3c(recon_bit.data_.data_, complex_im_recon_buf_, data_recon_buf_);
            }
            else
            {
                Gadgetron::hoNDFFT<float>::instance()->ifft2c(recon_bit.data_.data_, complex_im_recon_buf_, data_recon_buf_);
            }

            // SNR unit scaling
            size_t e1, e2, n, s;
            size_t num_readout_lines = 0;
            for (s = 0; s < S; s++)
            {
                for (n = 0; n < N; n++)
                {
                    for (e2 = 0; e2 < E2; e2++)
                    {
                        for (e1 = 0; e1 < E1; e1++)
                        {
                            if (std::abs(recon_bit.data_.data_(RO / 2, e1, e2, 0, n)) > 0)
                            {
                                num_readout_lines++;
                            }
                        }
                    }
                }
            }

            if (num_readout_lines > 0)
            {
                double lenRO = RO;

                size_t start_RO = recon_bit.data_.sampling_.sampling_limits_[0].min_;
                size_t end_RO = recon_bit.data_.sampling_.sampling_limits_[0].max_;

                if (!(start_RO<0 || end_RO<0 || (end_RO - start_RO + 1 == RO)))
                {
                    lenRO = (end_RO - start_RO + 1);
                }
                if (this->verbose.value()) GDEBUG_STREAM("GenericReconCartesianGrappaGadget, length for RO : " << lenRO << " - " << lenRO / RO);

                double effectiveAcceFactor = (double)(S*N*E1*E2) / (num_readout_lines);
                if (this->verbose.value()) GDEBUG_STREAM("GenericReconCartesianGrappaGadget, effectiveAcceFactor : " << effectiveAcceFactor);

                double ROScalingFactor = (double)RO / (double)lenRO;

                // since the grappa in gadgetron is doing signal preserving scaling, to perserve noise level, we need this compensation factor
                double grappaKernelCompensationFactor = 1.0 / (acceFactorE1_[e] * acceFactorE2_[e]);

                typename realType<T>::Type fftCompensationRatio = (typename realType<T>::Type)(std::sqrt(ROScalingFactor*effectiveAcceFactor) * grappaKernelCompensationFactor);

                if (this->verbose.value()) GDEBUG_STREAM("GenericReconCartesianGrappaGadget, fftCompensationRatio : " << fftCompensationRatio);

                Gadgetron::scal(fftCompensationRatio, complex_im_recon_buf_);
            }
            else
            {
                GWARN_STREAM("GenericReconCartesianGrappaGadget, cannot find any sampled lines ... ");
            }

            //float effectiveAcceFactor = acceFactorE1_[e] * acceFactorE2_[e];
            //if (effectiveAcceFactor > 1)
            //{
            //    float fftCompensationRatio = (float)(1.0 / std::sqrt(effectiveAcceFactor));
            //    Gadgetron::scal(fftCompensationRatio, complex_im_recon_buf_);
            //}

            //if (!debug_folder_full_path_.empty())
            //{
            //    std::stringstream os;
            //    os << "encoding_" << e;
            //    std::string suffix = os.str();
            //    gt_exporter_.exportArrayComplex(complex_im_recon_buf_, debug_folder_full_path_ + "aliasedIm_" + suffix);
            //}

            // unwrapping

            long long num = N*S*SLC;

            long long ii;

#pragma omp parallel default(none) private(ii) shared(num, N, S, RO, E1, E2, srcCHA, convkRO, convkE1, convkE2, ref_N, ref_S, recon_obj, dstCHA, e) if(num>1)
            {
#pragma omp for 
                for (ii = 0; ii < num; ii++)
                {
                    size_t slc = ii / (N*S);
                    size_t s = (ii - slc*N*S) / N;
                    size_t n = ii - slc*N*S - s*N;

                    // combined channels
                    T* pIm = &(complex_im_recon_buf_(0, 0, 0, 0, n, s, slc));
                    hoNDArray< std::complex<float> > aliasedIm(RO, E1, E2, srcCHA, 1, pIm);

                    size_t usedN = n;
                    if (n >= ref_N) usedN = ref_N - 1;

                    size_t usedS = s;
                    if (s >= ref_S) usedS = ref_S - 1;

                    T* pUnmix = &(recon_obj.unmixing_coeff_(0, 0, 0, 0, usedN, usedS, slc));
                    hoNDArray< std::complex<float> > unmixing(RO, E1, E2, srcCHA, pUnmix);

                    T* pRes = &(recon_obj.recon_res_.data_(0, 0, 0, 0, n, s, slc));
                    hoNDArray< std::complex<float> > res(RO, E1, E2, 1, pRes);

                    Gadgetron::apply_unmix_coeff_aliased_image_3D(aliasedIm, unmixing, res);
                }
            }

            /*if (!debug_folder_full_path_.empty())
            {
                std::stringstream os;
                os << "encoding_" << e;
                std::string suffix = os.str();
                gt_exporter_.exportArrayComplex(recon_obj.recon_res_.data_, debug_folder_full_path_ + "unwrappedIm_" + suffix);
            }*/
        }
        catch (...)
        {
            GADGET_THROW("Errors happened in GenericReconCartesianGrappaGadget::perform_unwrapping(...) ... ");
        }
    }

    GADGET_FACTORY_DECLARE(GenericReconCartesianGrappaGadget)
}